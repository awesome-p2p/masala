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
#include "send_p2p.h"
#include "lookup.h"
#include "announce.h"
#include "neighborhood.h"
#include "time.h"
#include "random.h"

LIST *nbhd_init( void ) {
	return bckt_init();
}

void nbhd_free( void ) {
	bckt_free( _main->nbhd );
}

void nbhd_put( UCHAR *id, IP *sa ) {
	ITEM *item_n = NULL;
	NODE *n = NULL;

	/* It's me */
	if( node_me( id ) ) {
		return;
	}

	if( (item_n = bckt_find_node( _main->nbhd, id )) != NULL ) {
		/* Node found */
		n = item_n->val;
		nbhd_update_address( n, sa );

	} else {
		n = (NODE *) myalloc( sizeof(NODE), "nbhd_put" );

		/* ID */
		memcpy( n->id, id, SHA_DIGEST_LENGTH );

		/* Timings */
		n->time_ping = time_add_5_min_approx();
		n->time_find = time_add_5_min_approx();
		n->pinged = 1;

		/* Update IP address */
		nbhd_update_address( n, sa );

		bckt_put( _main->nbhd, n );

		/* Send a PING */
		send_ping( &n->c_addr, SEND_UNICAST );

		/* New node: Ask for myself */
		send_find( &n->c_addr, _main->conf->node_id );
	}
}

void nbhd_del( NODE *n ) {
	bckt_del( _main->nbhd, n );
}

void nbhd_split( void ) {
	/* Do as many splits as neccessary */
	for( ;; ) {
		if( !bckt_split( _main->nbhd, _main->conf->node_id ) ) {
			return;
		}
	}
}

void nbhd_send( IP *sa, UCHAR *node_id, UCHAR *lkp_id, UCHAR *session_id, UCHAR *reply_type ) {
	ITEM *i = NULL;
	BUCK *b = NULL;

	if( (i = bckt_find_any_match( _main->nbhd, node_id )) == NULL ) {
		return;
	}
	b = i->val;

	send_node( sa, b, session_id, lkp_id, reply_type );
}

void nbhd_ping( void ) {
	ITEM *item_b = NULL;
	BUCK *b = NULL;
	ITEM *item_n = NULL;
	NODE *n = NULL;
	long int j = 0, k = 0;

	/* Cycle through all the buckets */
	item_b = _main->nbhd->start;
	for( k=0; k<_main->nbhd->counter; k++ ) {
		b = item_b->val;

		/* Cycle through all the nodes */
		item_n = b->nodes->start;
		for( j=0; j<b->nodes->counter; j++ ) {
			n = item_n->val;

			/* It's time for pinging */
			if( _main->p2p->time_now.tv_sec > n->time_ping ) {

				/* Ping the first 8 nodes. Sort out the rest. */
				if( j < 8 ) {
					send_ping( &n->c_addr, SEND_UNICAST );
				}

				nbhd_pinged( n->id );
			}

			item_n = list_next( item_n );
		}

		item_b = list_next( item_b );
	}
}

void nbhd_find_myself( void ) {
	nbhd_find( _main->conf->node_id );
}

void nbhd_find_random( void ) {
	UCHAR node_id[SHA_DIGEST_LENGTH];

	rand_urandom( node_id, SHA_DIGEST_LENGTH );
	nbhd_find( node_id );
}

void nbhd_find( UCHAR *find_id ) {
	ITEM *item_b = NULL;
	BUCK *b = NULL;
	ITEM *item_n = NULL;
	NODE *n = NULL;
	long int j = 0;

	if( (item_b = bckt_find_any_match( _main->nbhd, find_id )) != NULL ) {
		b = item_b->val;

		item_n = b->nodes->start;
		for( j=0; j<b->nodes->counter; j++ ) {
			n = item_n->val;

			/* Maintainance search */
			if( _main->p2p->time_now.tv_sec > n->time_find ) {

				send_find( &n->c_addr, find_id );
				n->time_find = time_add_5_min_approx();
			}

			item_n = list_next( item_n );
		}
	}
}

void nbhd_lookup( LOOKUP *l ) {
	ITEM *item_b = NULL;
	BUCK *b = NULL;
	ITEM *item_n = NULL;
	NODE *n = NULL;
	long int j = 0;
	long int max = 0;

	/* Find a matching bucket */
	if( (item_b = bckt_find_any_match( _main->nbhd, l->find_id )) == NULL ) {
		return;
	}

	/* Ask the first 8 nodes for the requested node */
	b = item_b->val;
	item_n = b->nodes->start;
	max = ( b->nodes->counter < 8 ) ? b->nodes->counter : 8;
	for( j = 0; j < max; j++ ) {
		n = item_n->val;

		/* Remember node */
		lkp_remember( l, n->id );

		/* Direct lookup */
		send_lookup( &n->c_addr, l->find_id, l->lkp_id );

		item_n = list_next( item_n );
	}
}

void nbhd_announce( ANNOUNCE *a, UCHAR *host_id ) {
	ITEM *item_b = NULL;
	BUCK *b = NULL;
	ITEM *item_n = NULL;
	NODE *n = NULL;
	long int j = 0;
	long int max = 0;

	/* Find a matching bucket */
	if( (item_b = bckt_find_any_match( _main->nbhd, host_id )) == NULL ) {
		return;
	}

	/* Ask the first 8 nodes */
	b = item_b->val;
	item_n = b->nodes->start;
	max = ( b->nodes->counter < 8 ) ? b->nodes->counter : 8;
	for( j = 0; j < max; j++ ) {
		n = item_n->val;

		/* Remember node */
		announce_remember( a, n->id );

		/* Send announcement */
		send_announce( &n->c_addr, a->lkp_id, host_id );

		item_n = list_next( item_n );
	}
}

void nbhd_pinged( UCHAR *id ) {
	ITEM *item_n = NULL;
	NODE *n = NULL;

	if( (item_n = bckt_find_node( _main->nbhd, id )) == NULL ) {
		return;
	}

	n = item_n->val;
	n->pinged++;

	/* ~5 minutes */
	n->time_ping = time_add_5_min_approx();
}

void nbhd_ponged( UCHAR *id, IP *sa ) {
	ITEM *item_n = NULL;
	NODE *n = NULL;

	if( (item_n = bckt_find_node( _main->nbhd, id )) == NULL ) {
		return;
	}

	n = item_n->val;
	n->pinged = 0;

	/* ~5 minutes */
	n->time_ping = time_add_5_min_approx();

	memcpy( &n->c_addr, sa, sizeof(IP) );
}

void nbhd_expire( void ) {
	ITEM *next = NULL;
	ITEM *item_b = NULL;
	BUCK *b = NULL;
	ITEM *item_n = NULL;
	NODE *n = NULL;
	long int j = 0, k = 0;

	/* Cycle through all the buckets */
	item_b = _main->nbhd->start;
	for( k=0; k<_main->nbhd->counter; k++ ) {
		b = item_b->val;

		/* Cycle through all the nodes */
		item_n = b->nodes->start;
		for( j=0; j<b->nodes->counter; j++ ) {
			n = item_n->val;
			next = list_next( item_n );

			/* Bad node */
			if( n->pinged >= 4 ) {
				/* Delete references */
				nbhd_del( n );
			}

			item_n = next;
		}

		item_b = list_next( item_b );
	}
}

/* Are all buckets empty? */
int nbhd_empty( void ) {
	ITEM *item_b;
	BUCK *b;
	long int k;

	/* Cycle through all the buckets */
	item_b = _main->nbhd->start;
	for( k=0; k<_main->nbhd->counter; k++ ) {
		b = item_b->val;

		if( b->nodes->counter > 0)
			return 0;

		item_b = list_next( item_b );
	}

	return 1;
}

void nbhd_update_address( NODE *n, IP *sa ) {
	if( memcmp( &n->c_addr, sa, sizeof(IP)) != 0 ) {
		memcpy( &n->c_addr, sa, sizeof(IP) );
	}
}
