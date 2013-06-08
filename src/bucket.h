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

struct obj_nodes {
	LIST *list;
	HASH *hash;
};
typedef struct obj_nodes NODES;

struct obj_node {
	IP c_addr;

	UCHAR id[SHA_DIGEST_LENGTH];

	time_t time_ping;
	time_t time_find;
	int pinged;
};
typedef struct obj_node NODE;

struct obj_neighborhood_bucket {
	UCHAR id[SHA_DIGEST_LENGTH];
	LIST *nodes;
};
typedef struct obj_neighborhood_bucket BUCK;

LIST *bckt_init( void );
void bckt_free( LIST *thislist );
void bckt_put( LIST *l, NODE *n );
void bckt_del( LIST *l, NODE *n );

ITEM *bckt_find_best_match( LIST *thislist, const UCHAR *id );
ITEM *bckt_find_any_match( LIST *thislist, const UCHAR *id );
ITEM *bckt_find_node( LIST *thislist, const UCHAR *id );

int bckt_split( LIST *thislist, const UCHAR *id );

int bckt_compute_id( LIST *thislist, ITEM *item_b, UCHAR *id_return );
int bckt_significant_bit( const UCHAR *id );

int node_me( UCHAR *node_id );
int node_equal( const UCHAR *node_a, const UCHAR *node_b );
