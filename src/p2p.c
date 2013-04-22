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
#include "opts.h"
#include "udp.h"
#include "ben.h"
#include "aes.h"
#include "node_p2p.h"
#include "bucket.h"
#include "lookup.h"
#include "announce.h"
#include "neighboorhood.h"
#include "p2p.h"
#include "send_p2p.h"
#include "search.h"
#include "cache.h"
#include "time.h"
#include "random.h"
#include "sha1.h"
#include "database.h"

struct obj_p2p *p2p_init( void ) {
	struct obj_p2p *p2p = (struct obj_p2p *) myalloc( sizeof(struct obj_p2p), "p2p_init" );

	p2p->time_maintainance = 0;
	p2p->time_multicast = 0;
	p2p->time_announce = 0;
	p2p->time_restart = 0;
	p2p->time_expire = 0;
	p2p->time_split = 0;
	p2p->time_find = 0;
	p2p->time_ping = 0;

	gettimeofday( &p2p->time_now, NULL );

	/* Worker Concurrency */
	p2p->mutex = mutex_init();

	return p2p;
}

void p2p_free( void ) {
	mutex_destroy( _main->p2p->mutex );
	myfree( _main->p2p, "p2p_free" );
}

void p2p_bootstrap( void ) {
	struct addrinfo hints;
	struct addrinfo *info = NULL;
	struct addrinfo *p = NULL;
	char buffer[MAIN_BUF+1];
	int rc = 0;
	int i = 0;

	log_info( "Connecting to a bootstrap server" );

	/* Compute address of bootstrap node */
	memset( &hints, '\0', sizeof(struct addrinfo) );
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET6;
	rc = getaddrinfo( _main->conf->bootstrap_node, _main->conf->bootstrap_port, &hints, &info );
	if( rc != 0 ) {
		snprintf( buffer, MAIN_BUF+1, "getaddrinfo: %s", gai_strerror( rc) );
		log_info( buffer );
		return;
	}

	p = info;
	while( p != NULL && i < XP2P_MAX_BOOTSTRAP_NODES ) {

		/* Send PING to a bootstrap node */
		if( strcmp( _main->conf->bootstrap_node, CONF_BOOTSTRAP_NODE) == 0 ) {
			send_ping( (IP *)p->ai_addr, SEND_MULTICAST );
		} else {
			send_ping( (IP *)p->ai_addr, SEND_UNICAST );
		}
		
		p = p->ai_next;	i++;
	}
	
	freeaddrinfo( info );
}

void p2p_parse( UCHAR *bencode, size_t bensize, IP *from ) {
	/* Tick Tock */
	mutex_block( _main->p2p->mutex );
	gettimeofday( &_main->p2p->time_now, NULL );
	mutex_unblock( _main->p2p->mutex );

	/* UDP packet too small */
	if( bensize < 1 ) {
		log_info( "UDP packet too small" );
		return;
	}

	/* Validate bencode */
	if( !ben_validate( bencode, bensize ) ) {
		log_info( "UDP packet contains broken bencode" );
		return;
	}

	/* Encrypted message or plaintext message */
	if( _main->conf->bool_encryption ) {
		p2p_decrypt( bencode, bensize, from );
	} else {
		p2p_decode( bencode, bensize, from );
	}
}

void p2p_decrypt( UCHAR *bencode, size_t bensize, IP *from ) {
	struct obj_ben *packet = NULL;
	struct obj_ben *salt = NULL;
	struct obj_ben *aes = NULL;
	struct obj_str *plain = NULL;

	/* Parse request */
	packet = ben_dec( bencode, bensize );
	if( !ben_is_dict( packet ) ) {
		log_info( "Decoding AES packet failed" );
		ben_free( packet );
		return;
	}

	/* Salt */
	salt = ben_searchDictStr( packet, "s" );
	if( !ben_is_str( salt ) || ben_str_size( salt ) != AES_IV_SIZE ) {
		log_info( "Salt missing or broken" );
		ben_free( packet );
		return;
	}

	/* Encrypted AES message */
	aes = ben_searchDictStr( packet, "a" );
	if( !ben_is_str( aes ) || ben_str_size( aes ) <= 2 ) {
		log_info( "AES message missing or broken" );
		ben_free( packet );
		return;
	}

	/* Decrypt message */
	plain = aes_decrypt( aes->v.s->s, aes->v.s->i,
		salt->v.s->s,
		_main->conf->key, strlen( _main->conf->key) );
	if( plain == NULL ) {
		log_info( "Decoding AES message failed" );
		ben_free( packet );
		return;
	}

	/* AES packet too small */
	if( plain->i < SHA_DIGEST_LENGTH ) {
		ben_free( packet );
		str_free( plain );
		log_info( "AES packet contains less than 20 bytes" );
		return;
	}

	/* Validate bencode */
	if( !ben_validate( plain->s, plain->i) ) {
		ben_free( packet );
		str_free( plain );
		log_info( "AES packet contains broken bencode" );
		return;
	}

	/* Parse message */
	p2p_decode( plain->s, plain->i, from );

	/* Free */
	ben_free( packet );
	str_free( plain );
}

void p2p_decode( UCHAR *bencode, size_t bensize, IP *from ) {
	struct obj_ben *packet = NULL;
	struct obj_ben *q = NULL;
	struct obj_ben *id = NULL;
	struct obj_ben *key = NULL;
	NODE *n = NULL;

	/* Parse request */
	packet = ben_dec( bencode, bensize );
	if( packet == NULL ) {
		log_info( "Decoding UDP packet failed" );
		return;
	} else if( packet->t != BEN_DICT ) {
		log_info( "UDP packet is not a dictionary" );
		ben_free( packet );
		return;
	}

	/* Node ID */
	id = ben_searchDictStr( packet, "i" );
	if( !p2p_is_hash( id ) ) {
		log_info( "Node ID missing or broken" );
		ben_free( packet );
		return;
	} else if( node_me( id->v.s->s) ) {
		if( node_counter() > 0 ) {
			/* Received packet from myself 
			 * If the node_counter is 0, 
			 * then you may see multicast requests from yourself.
			 * Do not warn about them.
			 */
			log_info( "WARNING: Received a packet from myself..." );
		}
		ben_free( packet );
		return;
	}

	/* Session key */
	key = ben_searchDictStr( packet, "k" );
	if( !p2p_is_hash( key ) ) {
		log_info( "Session key missing or broken" );
		ben_free( packet );
		return;
	}

	/* Remember node. This does not update the IP address. */
	n = node_put( id->v.s->s, (IP *)from );

	/* The neighboorhood */
	nbhd_put( n );

	/* Update IP if necessary. */
	node_update_address( n, (IP *)from );

	/* Query Details */
	q = ben_searchDictStr( packet, "q" );
	if( !ben_is_str( q ) || ben_str_size( q ) != 1 ) {
		log_info( "Query type missing or broken" );
		ben_free( packet );
		return;
	}
	switch( *q->v.s->s ) {

		/* Requests */
		case 'p':
			/* PING */
			mutex_block( _main->p2p->mutex );
			p2p_ping( key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		case 'f':
			/* FIND */
			mutex_block( _main->p2p->mutex );
			p2p_find( packet, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		case 'a':
			/* ANNOUNCE */
			mutex_block( _main->p2p->mutex );
			p2p_announce( packet, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		case 'l':
			/* LOOKUP */
			mutex_block( _main->p2p->mutex );
			p2p_lookup( packet, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;

		/* Replies */
		case 'o':
			/* PONG via PING */
			mutex_block( _main->p2p->mutex );
			p2p_pong( id->v.s->s, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		case 'F':
			/* NODES via FIND */
			mutex_block( _main->p2p->mutex );
			p2p_node_find( packet, id->v.s->s, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		case 'A':
			/* NODES via ANNOUNCE */
			mutex_block( _main->p2p->mutex );
			p2p_node_announce( packet, id->v.s->s, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		case 'L':
			/* NODES via LOOKUP */
			mutex_block( _main->p2p->mutex );
			p2p_node_lookup( packet, id->v.s->s, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		case 'V':
			/* VALUES via LOOKUP */
			mutex_block( _main->p2p->mutex );
			p2p_value( packet, id->v.s->s, key->v.s->s, from );
			mutex_unblock( _main->p2p->mutex );
			break;
		default:
			log_info( "Unknown query type" );
			ben_free( packet );
			return;
	}

	/* Free */
	ben_free( packet );
}

void p2p_cron( void ) {
	/* Tick Tock */
	gettimeofday( &_main->p2p->time_now, NULL );

	if( node_counter() == 0 ) {
		
		/* Bootstrap PING */
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_restart ) {
			p2p_bootstrap();
			_main->p2p->time_restart = time_add_2_min_approx();
		}
	
	} else {

		/* Expire objects every ~2 minutes */
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_expire ) {
			announce_expire();
			cache_expire();
			node_expire();
			lkp_expire();
			db_expire();
			_main->p2p->time_expire = time_add_2_min_approx();
		}

		/* Split container every ~2 minutes */
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_split ) {
			nbhd_split();
			_main->p2p->time_split = time_add_2_min_approx();
		}
	
		/* Ping all nodes every ~2 minutes */
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_ping ) {
			nbhd_ping();
			_main->p2p->time_ping = time_add_2_min_approx();
		}
	
		/* Find nodes every ~2 minutes */
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_find ) {
			nbhd_find_myself();
			_main->p2p->time_find = time_add_2_min_approx();
		}

		/* Find random node every ~2 minutes for maintainance reasons */
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_maintainance ) {
			nbhd_find_random();
			_main->p2p->time_maintainance = time_add_2_min_approx();
		}

		/* Announce my hostname every ~5 minutes */
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_announce ) {
			p2p_announce_myself();
			_main->p2p->time_announce = time_add_5_min_approx();
		}
	}

	/* Try to register multicast address until it works. */
	if( _main->udp->multicast == 0 ) {
		if( _main->p2p->time_now.tv_sec > _main->p2p->time_multicast ) {
			udp_multicast();
			_main->p2p->time_multicast = time_add_5_min_approx();
		}
	}
}

void p2p_ping( UCHAR *node_sk, IP *from ) {
	send_pong( from, node_sk );
}

void p2p_find( struct obj_ben *packet, UCHAR *node_sk, IP *from ) {
	struct obj_ben *ben_find_id = NULL;

	/* Find ID */
	ben_find_id = ben_searchDictStr( packet, "f" );
	if( !p2p_is_hash( ben_find_id ) ) {
		log_info( "Missing or broken target node" );
		return;
	}

	/* Reply */
	nbhd_send( from, ben_find_id->v.s->s, NULL, node_sk, (UCHAR *)"F");
}

void p2p_announce( struct obj_ben *packet, UCHAR *node_sk, IP *from ) {
	struct obj_ben *ben_host_id = NULL;
	struct obj_ben *ben_lkp_id = NULL;

	/* Host ID */
	ben_host_id = ben_searchDictStr( packet, "f" );
	if( !p2p_is_hash( ben_host_id ) ) {
		log_info( "Missing or broken target node" );
		return;
	}

	/* Lookup ID */
	ben_lkp_id = ben_searchDictStr( packet, "l" );
	if( !p2p_is_hash( ben_lkp_id ) ) {
		log_info( "Missing or broken lookup ID" );
		return;
	}

	/* Store announced host_id */
	db_put(ben_host_id->v.s->s, from);

	/* Reply nodes, that might suit even better */
	nbhd_send( from, ben_host_id->v.s->s, ben_lkp_id->v.s->s, node_sk, (UCHAR *)"A");
}

void p2p_lookup( struct obj_ben *packet, UCHAR *node_sk, IP *from ) {
	struct obj_ben *ben_find_id = NULL;
	struct obj_ben *ben_lkp_id = NULL;

	/* Find ID */
	ben_find_id = ben_searchDictStr( packet, "f" );
	if( !p2p_is_hash( ben_find_id ) ) {
		log_info( "Missing or broken target node" );
		return;
	}

	/* Lookup ID */
	ben_lkp_id = ben_searchDictStr( packet, "l" );
	if( !p2p_is_hash( ben_lkp_id ) ) {
		log_info( "Missing or broken lookup ID" );
		return;
	}

	/* Local database. */
	if ( !db_send( from, ben_find_id->v.s->s, ben_lkp_id->v.s->s, node_sk ) ) {

		/* Reply closer nodes */
		nbhd_send( from, ben_find_id->v.s->s, ben_lkp_id->v.s->s, node_sk, (UCHAR *)"L");
	}

}

void p2p_pong( UCHAR *node_id, UCHAR *node_sk, IP *from ) {
	if( !cache_validate( node_sk) ) {
		log_info( "Unexpected reply! Many answers to one multicast request?" );
		return;
	}

	/* Reply */
	node_ponged( node_id, from );
}

void p2p_node_find( struct obj_ben *packet, UCHAR *node_id, UCHAR *node_sk, IP *from ) {
	struct obj_ben *nodes = NULL;
	struct obj_ben *node = NULL;
	struct obj_ben *id = NULL;
	struct obj_ben *ip = NULL;
	struct obj_ben *po = NULL;
	ITEM *item = NULL;
	NODE *n = NULL;
	IP sin;
	long int i = 0;

	if( !cache_validate( node_sk ) ) {
		log_info( "Unexpected reply!" );
		return;
	}

	/* Requirements */
	nodes = ben_searchDictStr( packet, "n" );
	if( !ben_is_list( nodes ) ) {
		log_info( "NODE FIND: Nodes key broken or missing" );
		return;
	}

	/* Reply */
	item = nodes->v.l->start;
	for( i=0; i<nodes->v.l->counter; i++ ) {
		node = item->val;

		/* Node */
		if( !ben_is_dict( node ) ) {
			log_info( "Node key broken or missing" );
			return;
		}

		/* ID */
		id = ben_searchDictStr( node, "i" );
		if( !p2p_is_hash( id ) ) {
			log_info( "ID key broken or missing" );
			return;
		}

		/* IP */
		ip = ben_searchDictStr( node, "a" );
		if( !p2p_is_ip( ip ) ) {
			log_info( "IP key broken or missing" );
			return;
		}

		/* Port */
		po = ben_searchDictStr( node, "p" );
		if( !p2p_is_port( po ) ) {
			log_info( "Port key broken or missing" );
			return;
		}

		/* Compute source */
		memset( &sin, '\0', sizeof(IP) );
		sin.sin6_family = AF_INET6;
		memcpy( &sin.sin6_addr, ip->v.s->s, 16 );
		memcpy( &sin.sin6_port, po->v.s->s, 2 );

		/* Store node */
		if( !node_me( id->v.s->s) ) {
			n = node_put( id->v.s->s, ( IP *)&sin );
			node_update_address( n,( IP *)&sin );
			nbhd_put( n );
		}

		item = list_next( item );
	}
}

void p2p_node_announce( struct obj_ben *packet, UCHAR *node_id, UCHAR *node_sk, IP *from ) {
	struct obj_ben *nodes = NULL;
	struct obj_ben *node = NULL;
	struct obj_ben *id = NULL;
	struct obj_ben *ip = NULL;
	struct obj_ben *po = NULL;
	struct obj_ben *ben_lkp_id = NULL;
	ITEM *item = NULL;
	NODE *n = NULL;
	IP sin;
	long int i = 0;

	if( !cache_validate( node_sk ) ) {
		log_info( "Unexpected reply!" );
		return;
	}

	/* Requirements */
	nodes = ben_searchDictStr( packet, "n" );
	if( !ben_is_list( nodes ) ) {
		log_info( "NODE ANNOUNCE: Nodes key broken or missing" );
		return;
	}

	/* Lookup ID */
	ben_lkp_id = ben_searchDictStr( packet, "l" );
	if( !p2p_is_hash( ben_lkp_id ) ) {
		log_info( "Missing or broken lookup ID" );
		return;
	}

	/* Announce myself */
	announce_resolve( ben_lkp_id->v.s->s, node_id, from );

	/* Reply */
	item = nodes->v.l->start;
	for( i=0; i<nodes->v.l->counter; i++ ) {
		node = item->val;

		/* Node */
		if( !ben_is_dict( node ) ) {
			log_info( "Node key broken or missing" );
			return;
		}

		/* ID */
		id = ben_searchDictStr( node, "i" );
		if( !p2p_is_hash( id ) ) {
			log_info( "ID key broken or missing" );
			return;
		}

		/* IP */
		ip = ben_searchDictStr( node, "a" );
		if( !p2p_is_ip( ip ) ) {
			log_info( "IP key broken or missing" );
			return;
		}

		/* Port */
		po = ben_searchDictStr( node, "p" );
		if( !p2p_is_port( po ) ) {
			log_info( "Port key broken or missing" );
			return;
		}

		/* Compute source */
		memset( &sin, '\0', sizeof(IP) );
		sin.sin6_family = AF_INET6;
		memcpy( &sin.sin6_addr, ip->v.s->s, 16 );
		memcpy( &sin.sin6_port, po->v.s->s, 2 );

		/* Announce myself */
		announce_resolve( ben_lkp_id->v.s->s, id->v.s->s, &sin );

		/* Store node */
		if( !node_me( id->v.s->s) ) {
			n = node_put( id->v.s->s, (IP *)&sin );
			node_update_address( n, (IP *)&sin );
			nbhd_put( n );
		}

		item = list_next( item );
	}
}

void p2p_node_lookup( struct obj_ben *packet, UCHAR *node_id, UCHAR *node_sk, IP *from ) {
	struct obj_ben *nodes = NULL;
	struct obj_ben *node = NULL;
	struct obj_ben *id = NULL;
	struct obj_ben *ip = NULL;
	struct obj_ben *po = NULL;
	struct obj_ben *ben_lkp_id = NULL;
	ITEM *item = NULL;
	NODE *n = NULL;
	IP sin;
	long int i = 0;

	if( !cache_validate( node_sk ) ) {
		log_info( "Unexpected reply!" );
		return;
	}

	/* Requirements */
	nodes = ben_searchDictStr( packet, "n" );
	if( !ben_is_list( nodes ) ) {
		log_info( "LOOKUP: Nodes key broken or missing" );
		return;
	}

	/* Lookup ID */
	ben_lkp_id = ben_searchDictStr( packet, "l" );
	if( !p2p_is_hash( ben_lkp_id ) ) {
		log_info( "Missing or broken lookup ID" );
		return;
	}

	/* Lookup the requested hostname */
	lkp_resolve( ben_lkp_id->v.s->s, node_id, from );

	/* Reply */
	item = nodes->v.l->start;
	for( i=0; i<nodes->v.l->counter; i++ ) {
		node = item->val;

		/* Node */
		if( !ben_is_dict( node ) ) {
			log_info( "Node key broken or missing" );
			return;
		}

		/* ID */
		id = ben_searchDictStr( node, "i" );
		if( !p2p_is_hash( id ) ) {
			log_info( "ID key broken or missing" );
			return;
		}

		/* IP */
		ip = ben_searchDictStr( node, "a" );
		if( !p2p_is_ip( ip ) ) {
			log_info( "IP key broken or missing" );
			return;
		}

		/* Port */
		po = ben_searchDictStr( node, "p" );
		if( !p2p_is_port( po ) ) {
			log_info( "Port key broken or missing" );
			return;
		}

		/* Compute source */
		memset( &sin, '\0', sizeof(IP) );
		sin.sin6_family = AF_INET6;
		memcpy( &sin.sin6_addr, ip->v.s->s, 16 );
		memcpy( &sin.sin6_port, po->v.s->s, 2 );

		/* Lookup the requested hostname */
		lkp_resolve( ben_lkp_id->v.s->s, id->v.s->s, &sin );

		/* Store node */
		if( !node_me( id->v.s->s) ) {
			n = node_put( id->v.s->s, (IP *)&sin );
			node_update_address( n,( IP *)&sin );
			nbhd_put( n );
		}

		item = list_next( item );
	}
}

void p2p_value( struct obj_ben *packet, UCHAR *node_id, UCHAR *node_sk, IP *from ) {
	struct obj_ben *ben_address = NULL;
	struct obj_ben *ben_lkp_id = NULL;

	if( !cache_validate( node_sk ) ) {
		log_info( "Unexpected reply!" );
		return;
	}

	/* Requirements */
	ben_address = ben_searchDictStr( packet, "a" );
	if( !p2p_is_ip( ben_address ) ) {
		log_info( "Missing or broken lookup address" );
		return;
	}

	/* Lookup ID */
	ben_lkp_id = ben_searchDictStr( packet, "l" );
	if( !p2p_is_hash( ben_lkp_id ) ) {
		log_info( "Missing or broken lookup ID" );
		return;
	}

	/* Return IP to NSS */
	lkp_success(ben_lkp_id->v.s->s, ben_address->v.s->s);
}

void p2p_announce_myself( void ) {
	UCHAR lkp_id[SHA_DIGEST_LENGTH];

	/* Create random id to identify this search request */
	rand_urandom( lkp_id, SHA_DIGEST_LENGTH );

	/* Start find process */
	announce_put( lkp_id );
}

void p2p_compute_realm_id( UCHAR *host_id, char *hostname ) {
	char buffer[MAIN_BUF+1];

	/* The realm influences the way, the lookup hash gets computed */
	if( _main->conf->bool_realm == TRUE ) {
		snprintf(buffer, MAIN_BUF+1, "%s.%s", hostname, _main->conf->realm);
		sha1_hash( host_id, buffer, strlen(buffer) );
	} else {
		sha1_hash( host_id, hostname, strlen(hostname) );
	}
}

int p2p_is_hash( struct obj_ben *node ) {
	if( node == NULL ) {
		return 0;
	}
	if( !ben_is_str( node ) ) {
		return 0;
	}
	if( ben_str_size( node ) != SHA_DIGEST_LENGTH ) {
		return 0;
	}
	return 1;
}

int p2p_is_ip( struct obj_ben *node ) {
	if( node == NULL ) {
		return 0;
	}
	if( !ben_is_str( node ) ) {
		return 0;
	}
	if( ben_str_size( node ) != 16 ) {
		return 0;
	}
	return 1;
}

int p2p_is_port( struct obj_ben *node ) {
	if( node == NULL ) {
		return 0;
	}
	if( !ben_is_str( node ) ) {
		return 0;
	}
	if( ben_str_size( node ) != 2 ) {
		return 0;
	}
	return 1;
}
