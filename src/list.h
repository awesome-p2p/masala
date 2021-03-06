/*
Copyright 2006 Aiko Barz

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

struct obj_list {
	struct obj_item *start;
	struct obj_item *stop;
	long int counter;
};
typedef struct obj_list LIST;

struct obj_item {
	void *val;
	struct obj_item *next;
	struct obj_item *prev;
};
typedef struct obj_item ITEM;

LIST *list_init( void );
void list_free( LIST *list );
void list_clear( LIST *list );

ITEM *list_put( LIST *list, void *payload );
ITEM *list_ins( LIST *list, ITEM *here, void *payload );
ITEM *list_del( LIST *list, ITEM *item );

ITEM *list_next( ITEM *item );
ITEM *list_prev( ITEM *item );
void list_swap( LIST *list, ITEM *item1, ITEM *item2 );
