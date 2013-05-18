/*
Copyright 2013 Moritz Warning

This file is part of masala.

masala is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

masala is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with masala.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>

#include "thrd.h"
#include "main.h"
#include "str.h"
#include "list.h"
#include "hash.h"
#include "log.h"
#include "conf.h"
#include "ben.h"
#include "node_p2p.h"
#include "lookup.h"
#include "p2p.h"
#include "random.h"
#include "database.h"
#include "malloc.h"
#include "masala-web.h"


const char *reply_fmt = "HTTP/1.1 200 OK\r\n"
"Connection: close\r\n"
"Content-Length: %ul\r\n"
"Content-Type: text/plain\r\n"
"\r\n%s";

struct request {
	int clientfd;
	IP clientaddr;
};

void web_reply( void *ctx, UCHAR *id, UCHAR *address ) {
	char buffer[512];
	char addrbuf[INET6_ADDRSTRLEN+1];
	char hexbuf[HEX_LEN+1];
	struct request *request;

	request = (struct request *) ctx;

	if( address ) {
		inet_ntop( AF_INET6, address, addrbuf, sizeof(addrbuf) );
		sprintf( buffer, reply_fmt, strlen( addrbuf ), addrbuf );
		
		log_info( "Web: Answer request for '%s':\n%s", id_str( id, hexbuf ), addrbuf );

		sendto( request->clientfd, buffer, strlen(buffer), 0, (struct sockaddr*) &request->clientaddr, sizeof(IP) );
	}

	close( request->clientfd );
	myfree( request, "masalla-web" );
}

void web_lookup( CALLBACK *callback, void* ctx, UCHAR *id ) {
	IP *addr;

	/* Check my own DB for that node. */
	mutex_block( _main->p2p->mutex );
	addr = db_address( id );
	mutex_unblock( _main->p2p->mutex );

	if( addr != NULL ) {
		callback( ctx, id, (UCHAR *) &addr->sin6_addr );
		return;
	}

	/* Start find process */
	mutex_block( _main->p2p->mutex );
	lkp_put( id, callback, ctx );
	mutex_unblock( _main->p2p->mutex );
}

void* web_loop( void* _ ) {

	int rc;
	int val;
	struct addrinfo hints, *servinfo, *p;
	struct timeval tv;

	UCHAR id[SHA_DIGEST_LENGTH];
	char hexbuf[HEX_LEN+1];

	int sockfd, clientfd;
	IP sockaddr, clientaddr;
	char clientbuf[1500];
	struct request *request;
	char *hex_start, *hex_end;
	socklen_t addr_len = sizeof(IP);
	char addrbuf[FULL_ADDSTRLEN];

	const char *addr = "::1";
	const char *ifce = NULL;
	const char *port = "8080";

	memset( &hints, 0, sizeof(hints) );
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	if( (rc = getaddrinfo( addr, port, &hints, &servinfo )) == 0 ) {
		for( p = servinfo; p != NULL; p = p->ai_next ) {
			memset( &sockaddr, 0, sizeof(IP) );
			sockaddr = *((IP*) p->ai_addr);
			freeaddrinfo(servinfo);
			break;
		}
    } else {
		printf( "Web getaddrinfo failed: %s", gai_strerror(rc));
        return NULL;
	}

	if( (sockfd = socket( PF_INET6, SOCK_STREAM, IPPROTO_TCP )) < 0) {
		printf( "Web: Failed to create socket: %s", gai_strerror( errno ) );
		return NULL;
	}

	if( ifce && setsockopt( sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifce, strlen( ifce )) ) {
		printf( "Web: Unable to set interface '%s': %s", ifce,  gai_strerror( errno ) );
		return NULL;
	}

	val = 1;
	if( (rc = setsockopt( sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &val, sizeof(val) )) < 0 ) {
		log_err( "Web: Failed to set socket options: %s", gai_strerror(rc) );
		return NULL;
	}

	val = 1;
	setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val) );

	/* Set receive timeout */
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	if( (rc = setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv) )) < 0 ) {
		log_err( "Web: Failed to set socket options: %s", gai_strerror(rc) );
		return NULL;
	}

	if( (rc = bind( sockfd, (struct sockaddr*) &sockaddr, sizeof(IP) )) < 0 ) {
		log_err( "Web: Failed to bind socket to address: %s", gai_strerror(rc) );
		return NULL;
	}

	listen( sockfd, ntohs( sockaddr.sin6_port ) );

	log_info( "Bind Web interface to %s, interface %s.",
		addr ? addr_str( &sockaddr, addrbuf ) : "<any>",
		ifce ? ifce : "<any>"
	);

	clientfd = 0;
	while(1) {

		/* Close file descriptor that has not been used previously */
		if( clientfd > 0 )
			close( clientfd );

		clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &addr_len);
		rc = recv(clientfd, clientbuf, sizeof(clientbuf) - 1, 0);

		if(rc < 0)
			continue;

		/* Only handle GET requests. */
		if(rc < 6 || strncmp( "GET /", clientbuf, 5 ) != 0)
			continue;

		/* Jump after slash */
		hex_start = clientbuf + 5;

		clientbuf[rc] = ' ';
		hex_end = strchr( hex_start, ' ' );
		if( hex_end == NULL )
			continue;

		*hex_end = '\0';
		if( strlen( hex_start ) == 0 || strcmp( hex_start, "favicon.ico" ) == 0 )
			continue;

		/* That is the lookup key */
		p2p_compute_id( id, hex_start );
		log_info( "Web: Lookup '%s' as '%s'.", hex_start, id_str( id, hexbuf ) );

		request = (struct request *) myalloc( sizeof(struct request), "masalla-web"); 
		memcpy( &request->clientaddr, &clientaddr, sizeof(IP) );
		request->clientfd = clientfd;

		web_lookup( &web_reply, request, id );

		/* File descriptor is closed in callback */
		clientfd = 0;
	}

	return NULL;
}


int web_start()
{
	pthread_t tid;
	
	int rc = pthread_create( &tid, NULL, &web_loop, 0 );
	if (rc != 0) {
		log_crit( "Web: Failed to create a new thread." );
		return 1;
	}

	pthread_detach( tid );

	return 0;
}
