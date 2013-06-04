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

#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <stdarg.h>

#include "malloc.h"
#include "thrd.h"
#include "main.h"
#include "str.h"
#include "list.h"
#include "hash.h"
#include "log.h"
#include "conf.h"
#include "file.h"
#include "unix.h"
#include "udp.h"
#include "ben.h"
#include "p2p.h"
#include "node_p2p.h"
#include "bucket.h"
#include "send_p2p.h"
#include "lookup.h"
#include "announce.h"
#include "neighborhood.h"
#include "database.h"
#include "time.h"
#include "random.h"
#include "masala-cmd.h"


const char* cmd_usage_str = 
"Usage:\n"
"	ping <ip> [<port>]\n"
"	lookup <key>\n"
"	search <key>\n"
"	print_database\n"
"	print_nodes\n"
"	print_nbhd\n"
"	shutdown\n"
"\n";

void r_init( REPLY *r ) {
	r->data[0] = '\0';
	r->size = 0;
}

void r_printf( REPLY *r, const char *format, ... ) {
	va_list vlist;
	int written;

	va_start( vlist, format );
	written = vsnprintf( r->data + r->size, 1500 - r->size, format, vlist );
	va_end( vlist );

	if( written > 0 ) {
		r->size += written;
	} else {
		r->data[r->size] = '\0';
	}
}

/* Partition a string to the common argc/argv arguments */
void cmd_to_args(char *str, int* argc, char** argv, int max_argv) {
    int len, i;

    len = strlen(str);
    *argc = 0;

	/* Zero out white/control characters  */
    for (i = 0; i <= len; i++) {
        if( str[i] <= ' ')
            str[i] = '\0';
    }

	/* Record strings */
    for (i = 0; i <= len; i++) {
        if( str[i] == '\0')
			continue;

		if( *argc >= max_argv)
			break;

        argv[*argc] = &str[i];
        *argc = *argc + 1;
        i += strlen(&str[i]);
    }
}

int cmd_ping( REPLY *r, const char *addr, const char *port ) {
	struct addrinfo hints;
	struct addrinfo *info = NULL;
	struct addrinfo *p = NULL;
	char addrbuf[FULL_ADDSTRLEN+1];
	int rc = 0;

	/* Compute address */
	memset( &hints, '\0', sizeof(struct addrinfo) );
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET6;
	rc = getaddrinfo( addr, port, &hints, &info );
	if( rc != 0 ) {
		r_printf( r, "CMD: Failed to get address: %s", gai_strerror( rc ) );
		return 1;
	}

	r_printf( r, "Ping address %s", addr_str( (IP *)p->ai_addr, addrbuf ) );
	p = info;
	while( p != NULL ) {

		if( strcmp( _main->conf->bootstrap_node, CONF_BOOTSTRAP_NODE ) == 0 ) {
			/* Send PING to a bootstrap node */
			send_ping( (IP *)p->ai_addr, SEND_MULTICAST );
		} else {
			send_ping( (IP *)p->ai_addr, SEND_UNICAST );
		}

		p = p->ai_next;
	}

	freeaddrinfo( info );
	return 0;
}

void cmd_print_nbhd( REPLY *r ) {
	ITEM *item_b = NULL;
	BUCK *b = NULL;
	ITEM *item_n = NULL;
	NODE *n = NULL;
	long int j = 0, k = 0;
	char hexbuf[HEX_LEN+1];

	r_printf( r, "List of all buckets:\n" );

	/* Cycle through all the buckets */
	item_b = _main->nbhd->start;
	for( k=0; k<_main->nbhd->counter; k++ ) {
		b = item_b->val;

		r_printf( r, " Bucket: %s\n", id_str( b->id, hexbuf ) );

		/* Cycle through all the nodes */
		item_n = b->nodes->start;
		for( j=0; j<b->nodes->counter; j++ ) {
			n = item_n->val;

			r_printf( r, "  Node: %s\n", id_str( n->id, hexbuf ) );

			item_n = list_next( item_n );
		}

		item_b = list_next( item_b );
	}
}

void cmd_print_database( REPLY * r ) {
	ITEM *item_n = NULL;
	DB *n = NULL;
	char hexbuf[HEX_LEN+1];
	char addrbuf[FULL_ADDSTRLEN+1];
	long int k;

	r_printf( r, "Known host id / address pairs:\n" );

	item_n = _main->database->list->start;
	for( k = 0; k < _main->database->list->counter; k++ ) {
		n = item_n->val;

		r_printf( r, " %s /  %s\n", id_str( n->host_id, hexbuf ), addr_str( &n->c_addr, addrbuf ) );

		item_n = list_next( item_n );
	}

	r_printf( r, "Found %d entries.\n", k );
}

void cmd_print_nodes( REPLY * r ) {
	ITEM *item_n = NULL;
	NODE *n = NULL;
	char hexbuf[HEX_LEN+1];
	char addrbuf[FULL_ADDSTRLEN+1];
	long int k;

	r_printf( r, "Known node id / address pairs:\n" );

	item_n = _main->nodes->list->start;
	for( k = 0; k < _main->nodes->list->counter; k++ ) {
		n = item_n->val;

		r_printf( r, "%s /  %s\n", id_str( n->id, hexbuf ), addr_str( &n->c_addr, addrbuf ) );

		item_n = list_next( item_n );
	}

	r_printf( r, "Found %d entries.\n", k );
}

int cmd_exec( REPLY * r, int argc, char **argv ) {
	UCHAR id[SHA_DIGEST_LENGTH];
	char addrbuf[FULL_ADDSTRLEN+1];
	char hexbuf[HEX_LEN+1];
	IP *addr;
	int rc = 0;

	r_init( r );

	if( argc == 0 ) {
		/* print usage */
		r_printf( r, cmd_usage_str );
	} else if( strcmp( argv[0], "ping" ) == 0 && (argc == 2 || argc == 3) ) {

		const char *addr = argv[1];
		const char *port =  (argc == 3) ? argv[2] : CONF_PORT;
		rc = cmd_ping( r, addr, port );
	} else if( argc == 2 && strcmp( argv[0], "lookup" ) == 0 ) {

		/* That is the lookup key */
		p2p_compute_id( id, argv[1] );

		/* Check my own DB for that node. */
		mutex_block( _main->p2p->mutex );
		addr = db_address( id );
		mutex_unblock( _main->p2p->mutex );

		r_printf( r, "Lookup %s\n", id_str( id, hexbuf ) );
		if( addr != NULL ) {
			r_printf( r, "Address found: %s\n", addr_str( addr, addrbuf ) );
		} else {
			r_printf( r ,"No address found.\n" );
			rc = 1;
		}
	} else if( argc == 2 && strcmp( argv[0], "search" ) == 0 ) {

		/* That is the lookup key */
		p2p_compute_id( id, argv[1] );

		/* Start find process */
		mutex_block( _main->p2p->mutex );
		lkp_put( id, NULL, NULL );
		mutex_unblock( _main->p2p->mutex );

		r_printf( r, "Search started for %s.\n", id_str( id, hexbuf ) );
	} else if( strcmp( argv[0], "print_nbhd" ) == 0 ) {
		cmd_print_nbhd( r );
	} else if( strcmp( argv[0], "print_database" ) == 0 ) {
		cmd_print_database( r );
	} else if( strcmp( argv[0], "print_nodes" ) == 0 ) {
		cmd_print_nodes( r );
	} else if( strcmp( argv[0], "shutdown" ) == 0 ) {

		r_printf( r, "Shutting down now.\n" );
		_main->status = MAIN_SHUTDOWN;
	} else {
		/* print usage */
		r_printf( r, cmd_usage_str );
		rc = 1;
	}

	return rc;
}

void *cmd_remote_loop( void *_ ) {
	ssize_t rc;
	struct addrinfo hints, *servinfo, *p;
	struct timeval tv;

	char* argv[16];
	int argc;

	int sockfd;
    fd_set fds;
	IP clientaddr, sockaddr;
	socklen_t addrlen;
	char request[1500];
	REPLY reply;
	char addrbuf[FULL_ADDSTRLEN+1];

	memset( &hints, 0, sizeof(hints) );
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;

	if( (rc = getaddrinfo( CONF_CMD_ADDR, CONF_CMD_PORT, &hints, &servinfo )) == 0 ) {
		for( p = servinfo; p != NULL; p = p->ai_next ) {
			memset( &sockaddr, 0, sizeof(IP) );
			sockaddr = *((IP*) p->ai_addr);
			freeaddrinfo( servinfo );
			break;
		}
    } else {
		log_err( "CMD: getaddrinfo failed: %s", gai_strerror( rc ) );
        return NULL;
	}

	sockfd = socket( PF_INET6, SOCK_DGRAM, IPPROTO_UDP );
	if( sockfd < 0 ) {
		log_err( "CMD: Failed to create socket: %s", gai_strerror( errno ) );
		return NULL;
	}

	/* Set receive timeout */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	rc = setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv) );
	if( rc < 0 ) {
		log_err( "CMD: Failed to set socket option: %s", gai_strerror( rc ) );
		return NULL;
	}

	rc = bind( sockfd, (struct sockaddr*) &sockaddr, sizeof(IP) );
	if( rc < 0 ) {
		log_err( "CMD: Failed to bind socket to address: %s", gai_strerror( rc ) );
		return NULL;
	}

	log_info( "Bind CMD interface to %s.",
		addr_str( &sockaddr, addrbuf )
	);

    while( _main->status == MAIN_ONLINE ) {
		FD_ZERO( &fds );
		FD_SET( sockfd, &fds );

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		rc = select( sockfd+1, &fds, NULL, NULL, &tv );
		if( rc <= 0 ) {
			continue;
		}

		addrlen = sizeof(IP);
		rc = recvfrom( sockfd, request, sizeof(request) - 1, 0, (struct sockaddr*)&clientaddr, &addrlen );
		if( rc <= 0 ) {
			continue;
		}

		/* split up the command line into an argument array */
		cmd_to_args( request, &argc, &argv[0], sizeof(argv) );

		/* execute command line */
		cmd_exec( &reply, argc, argv );

		rc = sendto( sockfd, reply.data, reply.size, 0, (struct sockaddr *)&clientaddr, sizeof(IP) );
	}

	return NULL;
}

void cmd_console_loop() {
	char request[512];
	REPLY reply;
	char *argv[16];
	int argc;
	struct timeval tv;
    fd_set fds;
	int rc;

	printf( "Press Enter for help.\n" );

    while( _main->status == MAIN_ONLINE ) {
		FD_ZERO( &fds );
		FD_SET( STDIN_FILENO, &fds );

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		rc = select( STDIN_FILENO+1, &fds, NULL, NULL, &tv );

		if( rc == 0 ) {
			continue;
		}

		if( rc < 0 ) {
			break;
		}

		/* read line */
		fgets( request, sizeof(request), stdin );

		/* split up the command line into an argument array */
		cmd_to_args( request, &argc, &argv[0], sizeof(argv) );

		/* execute command line */
		cmd_exec( &reply, argc, argv );

		printf( "%.*s", (int) reply.size, reply.data );
    }
}

int cmd_remote_start( void ) {
	pthread_t tid;

	int rc = pthread_create( &tid, NULL, &cmd_remote_loop, 0 );
	if( rc != 0 ) {
		log_crit( "CMD: Failed to create thread." );
		return 1;
	}

	pthread_detach( tid );

	return 0;
}
