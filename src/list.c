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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/epoll.h>

#include "malloc.h"
#include "list.h"

LIST *list_init( void ) {
	LIST *list = (LIST *) myalloc( sizeof(LIST), "list_init" );

	list->start = NULL;
	list->stop = NULL;
	list->counter = 0;

	return list;
}

void list_free( LIST *list ) {
	if( list != NULL ) {
		while( list->start != NULL ) {
			list_del( list, list->start );
		}
		myfree( list, "list_free" );
	}
}

void list_clear( LIST *list ) {
	ITEM *item;

	/* Free payload */
	item = list->start;
	while( item ) {
		myfree( item->val, "list_clear" );
		item = list_next( item );
	}
}

ITEM *list_put( LIST *list, void *payload ) {
	ITEM *newItem = NULL;

	/* Overflow */
	if( list->counter+1 <= 0 ) {
		return NULL;
	}

	/* Get memory */
	newItem = (ITEM *) myalloc( sizeof(ITEM), "list_put" );

	/* Data container */
	newItem->val = payload;

	if(list->start == NULL ) {
		newItem->prev = NULL;
		newItem->next = NULL;

		list->start = newItem;
		list->stop = newItem;
	} else {
		newItem->prev = list->stop;
		newItem->next = NULL;

		list->stop->next = newItem;
		list->stop = newItem;
	}

	/* Increment counter */
	list->counter++;

	/* Return pointer to the new entry */
	return newItem;
}

ITEM *list_ins( LIST *list, ITEM *here, void *payload ) {
	ITEM *new = NULL;
	ITEM *next = NULL;

	/* Overflow */
	if( list->counter+1 <= 0 ) {
		return NULL;
	}

	/* This insert is like a normal list_put */
	if( list->counter <= 1 ) {
		return list_put( list, payload );
	}

	/* Data */
	new = (ITEM *) myalloc( sizeof(ITEM), "list_ins" );
	new->val = payload;

	/* Setup pointer */
	next = here->next;
	new->next = next;
	new->prev = here;
	here->next = new;
	if( next ) {
		next->prev = new;
	}

	/* Increment counter */
	list->counter++;

	/* Return pointer to the new entry */
	return new;
}

ITEM *list_del( LIST *list, ITEM *item ) {
	/* Variables */
	ITEM *prev = NULL;
	ITEM *next = NULL;

	/* Check input */
	if( list == NULL )
		return NULL;

	if( item == NULL )
		return NULL;

	if( list->counter <= 0 )
		return NULL;

	prev = item->prev;
	next = item->next;

	if( prev == NULL ) {
		list->start = next;
	} else {
		prev->next = next;
	}

	if( next == NULL ) {
		list->stop = prev;
	} else {
		next->prev = prev;
	}

	/* Decrement list counter */
	list->counter--;

	/* item is not linked anymore. Free it */
	myfree( item, "list_del" );

	return next;
}

ITEM *list_next( ITEM *item ) {
	/* Next item */
	return item->next;
}

ITEM *list_prev( ITEM *item ) {
	/* Previous item */
	return NULL;
}

void list_swap( LIST *list, ITEM *item1, ITEM *item2 ) {
	ITEM *prev1 = item1->prev;
	ITEM *next1 = item1->next;
	ITEM *prev2 = item2->prev;
	ITEM *next2 = item2->next;

	item1->prev = prev2;
	item1->next = next2;

	item2->prev = prev1;
	item2->next = next1;

	if( list->start == item1 ) {
		list->start = item2;
	} else if( list->start == item2 ) {
		list->start = item1;
	}

	if( list->stop == item1 ) {
		list->stop = item2;
	} else if( list->stop == item2 ) {
		list->stop = item1;
	}
}
