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
"	print_nbhd\n"
"	shutdown\n"
"\n";

/* partition a string to the common argc/argv arguments */
void cmd_to_args(char *str, int* argc, char** argv, int max_argv) {
    int len, i;

    len = strlen(str);
    *argc = 0;

    for (i = 0; i <= len; i++) {
        if( str[i] <= ' ')
            str[i] = '\0';
    }

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

int cmd_ping( int fd, const char *addr, const char *port ) {
	struct addrinfo hints;
	struct addrinfo *info = NULL;
	struct addrinfo *p = NULL;
	char addrbuf[FULL_ADDSTRLEN];
	int rc = 0;

	/* Compute address */
	memset( &hints, '\0', sizeof(struct addrinfo) );
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET6;
	rc = getaddrinfo( addr, port, &hints, &info );
	if( rc != 0 ) {
		dprintf( fd, "Failed to get address: %s", gai_strerror( rc ) );
		return 1;
	}

	dprintf( fd, "Ping address %s", addr_str( (IP *)p->ai_addr, addrbuf ) );
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

void cmd_print_nbhd( int fd ) {
	ITEM *item_b = NULL;
	BUCK *b = NULL;
	ITEM *item_n = NULL;
	NODE *n = NULL;
	long int j = 0, k = 0;
	char hexbuf[HEX_LEN+1];

	dprintf( fd, "Bucket split:\n" );

	/* Cycle through all the buckets */
	item_b = _main->nbhd->start;
	for( k=0; k<_main->nbhd->counter; k++ ) {
		b = item_b->val;

		dprintf( fd, " Bucket: %s\n", id_str( b->id, hexbuf ) );

		/* Cycle through all the nodes */
		item_n = b->nodes->start;
		for( j=0; j<b->nodes->counter; j++ ) {
			n = item_n->val;

			dprintf( fd, "  Node: %s\n", id_str( n->id, hexbuf ) );

			item_n = list_next( item_n );
		}

		item_b = list_next( item_b );
	}
}

int cmd_exec( int fd, int argc, char **argv ) {
	UCHAR id[SHA_DIGEST_LENGTH];
	char addrbuf[FULL_ADDSTRLEN];
	char hexbuf[HEX_LEN+1];
	IP *addr;
	int rc = 1;

    if( argc == 0 ) {

        rc = 1;
	} else if( strcmp( argv[0], "ping" ) == 0 && (argc == 2 || argc == 3) ) {

		const char *addr = argv[1];
		const char *port =  (argc == 3) ? argv[2] : CONF_PORT;
		rc = cmd_ping( fd, addr, port );
	} else if( argc == 2 && strcmp( argv[0], "lookup" ) == 0 ) {

		/* That is the lookup key */
		p2p_compute_id( id, argv[1] );

		/* Check my own DB for that node. */
		mutex_block( _main->p2p->mutex );
		addr = db_address( id );
		mutex_unblock( _main->p2p->mutex );

		dprintf( fd, "Lookup %s\n", id_str( id, hexbuf ) );
		if( addr != NULL ) {
			dprintf( fd, "Address found: %s\n", addr_str( addr, addrbuf ) );
		} else {
			dprintf( fd ,"No address found.\n" );
			return 1;
		}
		rc = 0;
	} else if( argc == 2 && strcmp( argv[0], "search" ) == 0 ) {

		/* That is the lookup key */
		p2p_compute_id( id, argv[1] );

		/* Start find process */
		mutex_block( _main->p2p->mutex );
		lkp_put( id, NULL, NULL );
		mutex_unblock( _main->p2p->mutex );

		dprintf( fd, "Search started for %s.\n", id_str( id, hexbuf ) );
		rc = 0;
	} else if( strcmp( argv[0], "print_nbhd" ) == 0 ) {

		cmd_print_nbhd( fd );
		rc = 0;
	} else if( strcmp( argv[0], "shutdown" ) == 0 ) {

		_main->status = MAIN_SHUTDOWN;
		rc = 0;
	}

	if( rc == 1) {
		/* print usage */
		dprintf( fd, cmd_usage_str );
	}

	return rc;
}

void cmd_socket_handler( int fd ) {
	char reqbuf[512];
	ssize_t len;
	char* argv[16];
	int argc;

	memset( reqbuf, 0, sizeof(reqbuf) );
	len = read( fd, reqbuf, sizeof(reqbuf) - 1 );

	if( len > 0 ) {

		/* split up the command line into an argument array */
		cmd_to_args( reqbuf, &argc, &argv[0], sizeof(argv) );

		/* execute command line */
		cmd_exec( fd, argc, argv );
	}

	shutdown( fd, 2 );
	close( fd );
}

void *cmd_socket_loop( void *_ ) {
	int sock, fd;
	struct sockaddr_un sa_un;
	socklen_t len;
	const char *sock_name = CONF_LOCAL_SOCK;

	memset( &sa_un, 0, sizeof(sa_un) );

	if( strlen( sock_name ) > (sizeof(sa_un.sun_path) - 1) ) {
		log_err( "CMD: Socket name too long." );
		return NULL;
	}

	sock = socket( PF_UNIX, SOCK_STREAM, 0 );

	/* delete socket file if it exists */
	unlink( sock_name );

	strcpy( sa_un.sun_path, sock_name );
	sa_un.sun_family = AF_UNIX;

	log_info( "CMD: Bind console to local socket: %s", sa_un.sun_path );

	if( bind( sock, (struct sockaddr *) &sa_un, strlen( sock_name ) + sizeof(sa_un.sun_family) ) ) {
		log_err( "CMD: Failed to bind control socket: %s", strerror( errno ) );
		return NULL;
	}

	if( listen( sock, 5 ) ) {
		log_err( "CMD: Failed to listen on control socket: %s", strerror( errno ) );
		return NULL;
	}

	while( _main->status == MAIN_ONLINE ) {
		memset( &sa_un, 0, sizeof(sa_un) );
		len = (socklen_t) sizeof(sa_un);
		if( (fd = accept( sock, (struct sockaddr *)&sa_un, &len) ) == -1 ) {
			log_err(  "CMD: Call to accept failed on local socket: %s", strerror( errno ) );
		} else {
			cmd_socket_handler( fd );
		}
	}

	return NULL;
}

void cmd_console_loop() {
    char buffer[512];
	char *argv[16];
	int argc;
	struct timeval tv;
    fd_set fds;
	int rc;

	dprintf( STDOUT_FILENO, "Press Enter for help.\n" );

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
		fgets( buffer, sizeof(buffer), stdin );

		/* split up the command line into an argument array */
		cmd_to_args( buffer, &argc, &argv[0], sizeof(argv) );

		/* execute command line */
		cmd_exec( STDOUT_FILENO, argc, argv );
    }
}

int cmd_start( void ) {
	pthread_t tid;

	int rc = pthread_create( &tid, NULL, &cmd_socket_loop, 0 );
	if( rc != 0 ) {
		log_crit( "DNS: Failed to create thread." );
		return 1;
	}

	pthread_detach( tid );

	return 0;
}
