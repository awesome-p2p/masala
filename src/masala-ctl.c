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
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>

#include "main.h"
#include "conf.h"

int main(int argc, char **argv) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct timeval tv;
	IP sockaddr;
	char buffer[1500];
	ssize_t rc;
	int i;

	memset( &hints, 0, sizeof(hints) );
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;

	if( (rc = getaddrinfo( CONF_CMD_ADDR, CONF_CMD_PORT, &hints, &servinfo )) == 0 ) {
		for( p = servinfo; p != NULL; p = p->ai_next ) {
			memset( &sockaddr, 0, sizeof(IP) );
			sockaddr = *((IP*) p->ai_addr);
			freeaddrinfo( servinfo );
			break;
		}
    } else {
		fprintf( stderr, "getaddrinfo failed: %s\n", gai_strerror( rc ) );
        return 1;
	}

	/* Construct request string from args */
    buffer[0] = '\0';
	for(i = 1; i < argc; ++i) {
		strcat(buffer, " ");
		strcat(buffer, argv[i]);
	}
	strcat(buffer, "\n");

	sockfd = socket( PF_INET6, SOCK_DGRAM, IPPROTO_UDP );
	if( sockfd < 0 ) {
		fprintf( stderr, "Failed to create socket: %s\n", strerror( errno ) );
		return 1;
	}

	/* Set receive timeout */
	tv.tv_sec = 0;
	tv.tv_usec = 200;
	rc = setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv) );
	if( rc < 0 ) {
		fprintf( stderr, "Failed to set socket option: %s\n", strerror( errno ) );
		return 1;
	}

	rc = sendto( sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&sockaddr, sizeof(IP) );
	if( rc <= 0 ) {
		fprintf( stderr, "Masala probably not started: %s\n", strerror( errno ) );
		return 1;
	}

	/* Receive reply */
	rc = read( sockfd, buffer, sizeof(buffer) - 1);

	if( rc <= 0 ) {
		printf("No response received.\n");
		return 1;
	}

	buffer[rc] = '\0';
	close( sockfd );

	if( buffer[0] == '0' ) {
		fprintf( stdout, buffer+1 );
		return 0;
	} else {
		fprintf( stderr, buffer+1 );
		return 1;
	}
}
