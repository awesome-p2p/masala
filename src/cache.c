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
#include "bucket.h"
#include "lookup.h"
#include "announce.h"
#include "neighborhood.h"
#include "cache.h"
#include "time.h"
#include "send_p2p.h"

struct obj_cache *cache_init( void ) {
	struct obj_cache *cache = (struct obj_cache *) myalloc( sizeof(struct obj_cache), "cache_init" );
	cache->list = list_init();
	cache->hash = hash_init( 100 );
	return cache;
}

void cache_free( void ) {
	list_clear( _main->cache->list );
	list_free( _main->cache->list );
	hash_free( _main->cache->hash );
	myfree( _main->cache, "cache_free" );
}

void cache_put( UCHAR *session_id, int type ) {
	ITEM *item_sk = NULL;
	struct obj_key *sk = NULL;

	if( hash_exists( _main->cache->hash, session_id, SHA_DIGEST_LENGTH) ) {
		return;
	}

	sk = (struct obj_key *) myalloc( sizeof(struct obj_key), "cache_put" );

	/* Session id */
	memcpy( sk->session_id, session_id, SHA_DIGEST_LENGTH );

	/* Multicast or Unicast */
	sk->type = type;

	/* Availability */
	sk->time = time_add_1_min();

	item_sk = list_put( _main->cache->list, sk );
	hash_put( _main->cache->hash, sk->session_id, SHA_DIGEST_LENGTH, item_sk );
}

void cache_del( UCHAR *session_id ) {
	ITEM *item_sk = NULL;
	struct obj_key *sk = item_sk->val;

	if( ( item_sk = hash_get( _main->cache->hash, session_id, SHA_DIGEST_LENGTH)) == NULL ) {
		return;
	}
	sk = item_sk->val;

	hash_del( _main->cache->hash, session_id, SHA_DIGEST_LENGTH );
	list_del( _main->cache->list, item_sk );
	myfree( sk, "cache_del" );
}

void cache_expire( void ) {
	ITEM *item_sk = NULL;
	ITEM *next_sk = NULL;
	struct obj_key *sk = NULL;

	item_sk = _main->cache->list->start;
	while( item_sk ) {
		sk = item_sk->val;
		next_sk = list_next( item_sk );

		/* Bad cache */
		if( _main->p2p->time_now.tv_sec > sk->time ) {
			cache_del( sk->session_id );
		}
		item_sk = next_sk;
	}
}

int cache_validate( UCHAR *session_id ) {
	ITEM *item_sk = NULL;
	struct obj_key *sk = NULL;

	/* Key not found */
	if( ( item_sk = hash_get( _main->cache->hash, session_id, SHA_DIGEST_LENGTH)) == NULL ) {
		return 0;
	}
	sk = item_sk->val;

	/* Unicast: 
	 *  Delete session id
	 *
	 * Multicast:
	 *  Ignore session id, because we will receive multiple answers with same session id.
	 *  The session id will timeout later...
	 */
	if( sk->type == SEND_UNICAST ) {
		cache_del( session_id );
	}

	return 1;
}
