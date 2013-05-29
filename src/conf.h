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

#define CONF_CORES 2
#define CONF_PORTMIN 1
#define CONF_PORTMAX 65535

#define CONF_BEQUIET 0
#define CONF_VERBOSE 1

#define CONF_DAEMON 0
#define CONF_FOREGROUND 1

#define CONF_USER "masala"
#define CONF_EPOLL_WAIT 2000
#define CONF_SRVNAME "masala"
#define CONF_PORT "8337"
#define CONF_BOOTSTRAP_NODE "ff0e::1"
#define CONF_BOOTSTRAP_PORT "8337"
#define CONF_DNS_ADDR "::1"
#define CONF_DNS_PORT "3444"
#define CONF_WEB_ADDR "::1"
#define CONF_WEB_PORT "8080"
#define CONF_CMD_ADDR "::1"
#define CONF_CMD_PORT "4374"

struct obj_conf {
	char *user;

	char *pid_file;
	char *hostname;
	UCHAR node_id[SHA_DIGEST_LENGTH];
	UCHAR host_id[SHA_DIGEST_LENGTH];
	UCHAR null_id[SHA_DIGEST_LENGTH];
	char *bootstrap_node;
	char *bootstrap_port;

#ifdef DNS
	char *dns_port;
	char *dns_addr;
	char *dns_ifce;
#endif
#ifdef WEB
	char *web_port;
	char *web_addr;
	char *web_ifce;
#endif

	/* Number of cores */
	int cores;

	/* Verbosity */
	int quiet;

	/* Verbosity mode */
	int mode;

	/* TCP/UDP Port */
	char *port;

	/* Limit communication to this interface */
	char *interface;
};

struct obj_conf *conf_init( void );
void conf_free( void );

void conf_check( void );
