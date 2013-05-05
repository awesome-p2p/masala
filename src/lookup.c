/*
Copyright 2011 Aiko Barz

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
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <netdb.h>
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
#include "time.h"
#include "lookup.h"
#include "announce.h"
#include "node_p2p.h"
#include "neighboorhood.h"
#include "bucket.h"
#include "send_p2p.h"
#include "random.h"

LOOKUPS *lkp_init( void ) {
	LOOKUPS *lookups = (LOOKUPS *) myalloc( sizeof(LOOKUPS), "lkp_init" );
	lookups->list = list_init();
	lookups->hash = hash_init( 4096 );
	return lookups;
}

void lkp_free( void ) {
	list_clear( _main->lkps->list );
	list_free( _main->lkps->list );
	hash_free( _main->lkps->hash );
	myfree( _main->lkps, "lkp_free" );
}

LOOKUP *lkp_put( UCHAR *find_id, CALLBACK *callback, void *ctx ) {
	ITEM *i = NULL;
	LOOKUP *l = NULL;

	l = (LOOKUP *) myalloc( sizeof(LOOKUP), "lkp_put" );

	/* Remember nodes that have been asked */
	l->list = list_init();
	l->hash = hash_init( 100 );

	/* ID */
	memcpy( l->find_id, find_id, SHA_DIGEST_LENGTH );

	/* Create random id to identify this search request */
	rand_urandom( l->lkp_id, SHA_DIGEST_LENGTH );

	/* Callback on success */
	l->callback = callback;
	l->ctx = ctx;

	/* Timings */
	l->time_find = 0;

	/* Remember lookup request */
	i = list_put( _main->lkps->list, l );
	hash_put( _main->lkps->hash, l->lkp_id, SHA_DIGEST_LENGTH, i );

	/* Search the requested name */
	nbhd_lookup( l );

	return l;
}

void lkp_del( ITEM *i ) {
	LOOKUP *l = i->val;

	/* Let the callback know the lookup timed out */
	if(l->callback)
		l->callback( l->ctx, l->find_id, NULL );

	/* Free lookup cache */
	list_clear( l->list );
	list_free( l->list );
	hash_free( l->hash );

	/* Delete lookup item */
	hash_del( _main->lkps->hash, l->lkp_id, SHA_DIGEST_LENGTH );
	list_del( _main->lkps->list, i );
	myfree( l, "lkp_del" );
}

void lkp_expire( void ) {
	ITEM *i = NULL;
	ITEM *next = NULL;
	LOOKUP *l = NULL;
	long int j = 0;

	i = _main->lkps->list->start;
	for( j=0; j<_main->lkps->list->counter; j++ ) {
		l = i->val;
		next = list_next( i );

		if( _main->p2p->time_now.tv_sec > l->time_find ) {
			lkp_del( i );
		}
		i = next;
	}
}

void lkp_resolve( UCHAR *lkp_id, UCHAR *node_id, IP *c_addr ) {
	ITEM *i = NULL;
	LOOKUP *l = NULL;

	/* Lookup the lookup ID */
	if( ( i = hash_get( _main->lkps->hash, lkp_id, SHA_DIGEST_LENGTH ) ) == NULL ) {
		return;
	}
	l = i->val;

	/* Ask every node only once */
	if( hash_exists( l->hash, node_id, SHA_DIGEST_LENGTH ) ) {
		return;
	}

	/* Ask the node just once */
	if( !node_me( node_id ) ) {
		send_lookup( c_addr, l->find_id, lkp_id );
	}

	/* Remember that node */
	lkp_remember( l, node_id );
}

void lkp_success( UCHAR *lkp_id, UCHAR *node_id, UCHAR *address ) {
	ITEM *i = NULL;
	LOOKUP *l = NULL;

	/* Lookup the lookup ID */
	if( ( i = hash_get( _main->lkps->hash, lkp_id, SHA_DIGEST_LENGTH ) ) == NULL ) {
		return;
	}
	l = i->val;

	if(l->callback)
		l->callback( l->ctx, node_id, address );

	/* Done */
	lkp_del( i );
}

void lkp_remember( LOOKUP *l, UCHAR *node_id ) {
	UCHAR *buffer = NULL;

	/* Remember that node */
	buffer = (UCHAR *) myalloc( SHA_DIGEST_LENGTH*sizeof(char), "lkp_remember" );
	memcpy( buffer, node_id, SHA_DIGEST_LENGTH );
	list_put( l->list, buffer );
	hash_put( l->hash, buffer, SHA_DIGEST_LENGTH, buffer );
}
