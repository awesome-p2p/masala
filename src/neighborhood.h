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

LIST *nbhd_init( void );
void nbhd_free( void );

void nbhd_put( UCHAR *id, IP *sa );
void nbhd_del( NODE *n );

void nbhd_split( void );
void nbhd_ping( void );

void nbhd_find_myself( void );
void nbhd_find_random( void );
void nbhd_find( UCHAR *find_id );
void nbhd_lookup( LOOKUP *l );
void nbhd_announce( ANNOUNCE *a, UCHAR *host_id );

void nbhd_send( IP *sa, UCHAR *node_id, UCHAR *lkp_id, UCHAR *session_id, UCHAR *reply_type );
void nbhd_print( void );

void nbhd_pinged( UCHAR *id );
void nbhd_ponged( UCHAR *id, IP *sa );

void nbhd_expire( void );

int nbhd_empty( void );
void nbhd_update_address( NODE *n, IP *sa );
