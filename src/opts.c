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
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "malloc.h"
#include "main.h"
#include "conf.h"
#include "str.h"
#include "hash.h"
#include "list.h"
#include "log.h"
#include "file.h"
#include "opts.h"
#include "node_p2p.h"
#include "search.h"
#include "ben.h"
#include "p2p.h"

const char *usage = "Masala - A P2P name resolution daemon (IPv6 only)\n"
"A Distributed Hashtable (DHT) combined with a basic DNS-server interface.\n\n"
"Usage: masala [OPTIONS]...\n"
"\n"
" -h, --hostname		Set the hostname the node should announce (Default: <hostname>.p2p).\n"
" -u, --user		Change the UUID after start.\n"
" -ba, --boostrap-addr	Use another node to connect to the masala network (Default: 'ff0e::1').\n"
" -bp, --boostrap-port	Set the port for the bootsrap node (Default: UDP/8337).\n"
" -p, --port		Bind to this port (Default: UDP/8337).\n"
" -i, --interface	Bind to this interface (Default: <any>).\n"
" -d, --daemon		Run the node in background.\n"
" -q, --quiet		Be quiet and do not log anything.\n"
" -pf, --pid-file	Write process pid to a file.\n"
#ifdef DNS
" -da, --dns-addr	Bind the DNS server to this address (Default: '::1').\n"
" -dp, --dns-port	Bind the DNS server to this port (Default: 3444).\n"
" -di, --dns-ifce	Bind the DNS server to this interface (Default: <any>).\n"
#endif
#ifdef WEB
" -wa, --web-addr	Bind the WEB server to this address (Default: '::1').\n"
" -wp, -web-port	Bind the WEB server to this port (Default: 8080).\n"
" -wi, --web-ifce	Bind the WEB server to this interface (Default: <any>).\n"
#endif
"\n"
"Example: masala -h fubar.p2p -k fubar\n"
"\n";

void opts_load( int argc, char **argv ) {
	unsigned int i;

	if( argv == NULL ) {
		return;
	}

	for( i=1; i<argc; i++ ) {
		if( argv[i] != NULL && argv[i][0] == '-' ) {
			if( i+1 < argc && argv[i+1] != NULL && argv[i+1][0] != '-' ) {
				/* -x abc */
				opts_interpreter( argv[i], argv[i+1] );
				i++;
			} else {
				/* -x -y => -x */
				opts_interpreter( argv[i], NULL );
			}
		}
	}
}


void arg_expected( const char *var ) {
	log_err( "Argument expected for option %s.", var );
}

void no_arg_expected( const char *var ) {
	log_err( "No argument expected for option %s.", var );
}

int match( const char *opt, const char *opt1, const char *opt2 ) {
	return (strcmp( opt, opt1 ) == 0 || strcmp( opt, opt2 ) == 0);
}

/* free the old string and set the new */
void replace( char *var, char** dst, char *src ) {
	if( src == NULL )
		arg_expected( var );

	myfree( *dst, "opts_replace" );
	*dst = strdup( src );
}

void opts_interpreter( char *var, char *val ) {

	if( match( var, "-ba", "--bootstrap-addr") ) {
		replace( var, &_main->conf->bootstrap_node, val );
	} else if( match( var, "-bp", "--bootstrap-port" ) ) {
		replace( var, &_main->conf->bootstrap_port, val );
	} else if( match( var, "-pf", "--pid-file" ) ) {
		replace( var, &_main->conf->pid_file, val );
	} else if( match( var, "-h", "--hostname" ) ) {
		replace( var, &_main->conf->hostname, val );

		/* Compute host_id. */
		p2p_compute_id( _main->conf->host_id, _main->conf->hostname );
	} else if( match( var, "-q", "--quiet" ) ) {
		if( val != NULL )
			no_arg_expected( var );
		_main->conf->quiet = CONF_BEQUIET;
#ifdef DNS
	} else if( match( var, "-dp", "--dns-port" ) ) {
		replace( var, &_main->conf->dns_port, val );
	} else if( match( var, "-da", "--dns-addr" ) ) {
		replace( var, &_main->conf->dns_addr, val );
	} else if( match( var, "-di", "--dns-ifce" ) ) {
		replace( var, &_main->conf->dns_ifce, val );
#endif
#ifdef WEB
	} else if( match( var, "-wp", "--web-port" ) ) {
		replace( var, &_main->conf->web_port, val );
	} else if( match( var, "-wa", "--web-addr" ) ) {
		replace( var, &_main->conf->web_addr, val );
	} else if( match( var, "-wi", "--web-ifce" ) ) {
		replace( var, &_main->conf->web_ifce, val );
#endif
	} else if( match( var, "-p", "--port" ) ) {
		replace( var, &_main->conf->port, val );
	} else if( match( var, "-i", "--interface" ) ) {
		replace( var, &_main->conf->interface, val );
	} else if( match( var, "-u", "--user" ) ) {
		replace( var, &_main->conf->user, val );
	} else if( match( var, "-d", "--daemon" ) ) {
		if( val != NULL)
			no_arg_expected( var );
		_main->conf->mode = CONF_DAEMON;
	} else if( match( var, "--help", "" ) ) {
		printf( usage );
		exit( 0 );
	} else {
		log_err( "Unknown command line option '%s'", var );
	}
}
