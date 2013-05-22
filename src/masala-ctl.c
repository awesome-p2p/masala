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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>

#include "main.h"
#include "conf.h"

int connect_to_server(char *sock_name)
{
	int sock;
	struct sockaddr_un addr;

	sock = socket( AF_UNIX, SOCK_STREAM, 0 );
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy( addr.sun_path, sock_name, (sizeof(addr.sun_path) - 1) );

	if( connect( sock, (struct sockaddr *)&addr, strlen(addr.sun_path) + sizeof(addr.sun_family) ) ) {
		fprintf( stderr, "Masala probably not started: %s\n", strerror( errno ) );
		exit( 1 );
	}

	return sock;
}

int send_request(int sock, char *request) {
	ssize_t	len, written;

	len = 0;
	while( len != strlen( request ) ) {
		written = write( sock, (request + len), strlen( request ) - len );
		if( written == -1 ) {
			fprintf( stderr, "Write to socket failed: %s\n", strerror( errno ) );
			exit( 1 );
		}
		len += written;
	}

	return (int) len;
}


int main(int argc, char **argv) {
	int sock;
	char buffer[512];
	int len;
	int i;

	sock = connect_to_server( strdup( CONF_LOCAL_SOCK ) );

    buffer[0] = '\0';
	for(i = 1; i < argc; ++i) {
		strcat(buffer, " ");
		strcat(buffer, argv[i]);
	}

	len = send_request( sock, buffer );

	while( (len = read( sock, buffer, sizeof(buffer) - 1) ) > 0 ) {
		buffer[len] = '\0';
		printf( "%s\n", buffer );
	}

	if( len < 0 ) {
		fprintf( stderr, "Error reading socket: %s", strerror( errno ) );
	}

	shutdown( sock, 2 );
	close( sock );

	return 0;
}
