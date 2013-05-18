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
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include <syslog.h>
#include <stdarg.h>

#include "malloc.h"
#include "main.h"
#include "conf.h"
#include "str.h"
#include "thrd.h"
#include "list.h"
#include "log.h"

char* id_str( const UCHAR *in, char *buf ) {
	long int i = 0;
	UCHAR *p0 = (UCHAR *)in;
	char *p1 = buf;

	memset( buf, '\0', HEX_LEN+1 );

	for( i=0; i<SHA_DIGEST_LENGTH; i++ ) {
		snprintf( p1, 3, "%02x", *p0 );
		p0++;
		p1+=2;
	}

	return buf;
}

char* addr_str( IP *addr, char *addrbuf ) {
	char buf[INET6_ADDRSTRLEN+1];
	unsigned short port = ntohs( addr->sin6_port );

	inet_ntop( AF_INET6, &addr->sin6_addr, buf, sizeof(buf) );
	sprintf(addrbuf, "[%s]:%d", buf, port);

	return addrbuf;
}

void _log( const char *filename, int line, int priority, const char *format, ... ) {
	char buffer[MAIN_BUF];
	va_list vlist;

	va_start( vlist, format );
	vsnprintf( buffer, MAIN_BUF, format, vlist );
	va_end( vlist );

	if( priority == LOG_INFO && (_main->conf->quiet == CONF_BEQUIET) ) {
		return;
	}

	if( _main->conf->mode == CONF_FOREGROUND ) {
		if(filename)
			fprintf( stderr, "(%s:%d) %s\n", filename, line, buffer );
		else
			fprintf( stderr, "%s\n", buffer );
	} else {
		openlog( CONF_SRVNAME, LOG_PID|LOG_CONS, LOG_USER|LOG_PERROR );
		if(filename)
			syslog( priority, "(%s:%d) %s", filename, line, buffer );
		else
			syslog( priority, "%s", buffer );
		closelog();
	}

	if( priority == LOG_CRIT || priority == LOG_ERR )
		exit( 1 );
}
