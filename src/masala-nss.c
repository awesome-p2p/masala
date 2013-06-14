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

#include "thrd.h"
#include "main.h"
#include "str.h"
#include "list.h"
#include "hash.h"
#include "log.h"
#include "conf.h"
#include "ben.h"
#include "lookup.h"
#include "p2p.h"
#include "random.h"
#include "database.h"
#include "malloc.h"
#include "masala-nss.h"

void nss_reply( int sockfd, IP *clientaddr, UCHAR *node_id, IP* node_addr ) {
	UCHAR reply[SHA_DIGEST_LENGTH+16];

	memcpy( reply, node_id, SHA_DIGEST_LENGTH );
	memcpy( reply+SHA_DIGEST_LENGTH, (UCHAR *) &node_addr->sin6_addr.s6_addr[0], 16 );

	sendto( sockfd, reply, sizeof(reply), 0, (const struct sockaddr *) clientaddr, sizeof( IP ) );
}

void nss_lookup( int sockfd, IP *clientaddr, UCHAR *node_id ) {
	IP *node_addr;

	/* Check my own DB for that node. */
	mutex_block( _main->p2p->mutex );
	node_addr = db_address( node_id );
	mutex_unblock( _main->p2p->mutex );

	if( node_addr != NULL ) {
		nss_reply( sockfd, clientaddr, node_id, node_addr );
		return;
	}

	/* Start find process */
	mutex_block( _main->p2p->mutex );
	lkp_put( node_id, NULL, NULL );
	mutex_unblock( _main->p2p->mutex );
}

/*
listen for local connection
*/
void* nss_loop( void* _ ) {

	ssize_t rc;
	struct addrinfo hints, *servinfo, *p;
	struct timeval tv;

	int sockfd;
	IP clientaddr, sockaddr;
	char hostname[256];
	UCHAR host_id[SHA_DIGEST_LENGTH];
	char addrbuf[FULL_ADDSTRLEN+1];
	socklen_t addr_len = sizeof(IP);

	memset( &hints, 0, sizeof(hints) );
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;

	if( (rc = getaddrinfo( "::1", NULL, &hints, &servinfo )) == 0 ) {
		for( p = servinfo; p != NULL; p = p->ai_next ) {
			memset( &sockaddr, 0, sizeof(IP) );
			sockaddr = *((IP*) p->ai_addr);
			sockaddr.sin6_port = htons( MASALA_NSS_PORT );
			freeaddrinfo( servinfo );
			break;
		}
    } else {
		log_err( "NSS: getaddrinfo failed: %s", gai_strerror( rc ) );
        return NULL;
	}

	sockfd = socket( PF_INET6, SOCK_DGRAM, IPPROTO_UDP );
	if( sockfd < 0 ) {
		log_err( "NSS: Failed to create socket: %s", strerror( errno ) );
		return NULL;
	}

	/* Set receive timeout */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	rc = setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv) );
	if( rc < 0 ) {
		log_err( "NSS: Failed to set socket option: %s", strerror( errno ) );
		return NULL;
	}

	rc = bind( sockfd, (struct sockaddr*) &sockaddr, sizeof(IP) );
	if( rc < 0 ) {
		log_err( "NSS: Failed to bind socket to address: %s", strerror( errno ) );
		return NULL;
	}

	log_info( "NSS: Bind socket to %s.",
		addr_str( &sockaddr, addrbuf )
	);

	while( 1 ) {

		rc = recvfrom( sockfd, hostname, sizeof( hostname ), 0, (struct sockaddr *) &clientaddr, &addr_len );

		if( rc <= 0 || rc >= 256 ) {
			continue;
		}

		hostname[rc] = '\0';

		/* Validate hostname */
		if ( !str_isValidHostname( (char*) hostname, strlen( hostname ) ) ) {
			log_warn( "NSS: Invalid hostname for lookup: '%s'", hostname );
			continue;
		}

		/* That is the lookup key */
		p2p_compute_id( host_id, hostname );

		nss_lookup( sockfd, &clientaddr, host_id );
	}

	return NULL;
}

int nss_start( void )
{
	pthread_t tid;

	int rc = pthread_create( &tid, NULL, &nss_loop, 0 );
	if( rc != 0 ) {
		log_crit( "NSS: Failed to create a new thread." );
		return 1;
	}

	pthread_detach( tid );

	return 0;
}
