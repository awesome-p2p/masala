/*
Copyright 2006 Aiko Barz

This file is part of masala/tumbleweed.

masala/tumbleweed is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

masala/tumbleweed is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with masala/tumbleweed.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef TUMBLEWEED
#include "malloc.h"
#include "main.h"
#include "conf.h"
#include "str.h"
#include "hash.h"
#include "list.h"
#include "node_web.h"
#include "log.h"
#include "file.h"
#include "opts.h"
#else
#include "malloc.h"
#include "main.h"
#include "conf.h"
#include "str.h"
#include "hash.h"
#include "list.h"
#include "log.h"
#include "file.h"
#include "opts.h"
#include "sha1.h"
#include "node_p2p.h"
#include "search.h"
#include "ben.h"
#include "p2p.h"
#endif


const char *usage = "Masala - A P2P name resolution daemon (IPv6 only)\n"
"A Distributed Hashtable (DHT) combined with a basic DNS-server interface.\n\n"
"Usage: masala [OPTIONS]...\n"
"\n"
" -h, --hostname		Set the hostname the node should announce (Default: <hostname>.p2p).\n"
" -u, --username		Change the UUID after start.\n"
" -k, --key		Set a password results in encrypting each packet with AES256.\n"
" -r, --realm		Salt your hostname before its hash is used as node ID.\n"
" -ba, --boostrap-addr	Use another node to connect to the masala network (Default: 'ff0e::1').\n"
" -bp, --boostrap-port	Set the port for the bootsrap node (Default: UDP/8337).\n"
" -p, --port		Set the port for the node (Default: UDP/8337).\n"
" -d, --daemon		Run the node in background.\n"
" -q, --quiet		Be quiet and do not log anything.\n"
" -da, --dns-addr	Set the address for the DNS Server interface to listen to (Default: '::1').\n"
" -dp, --dns-port	Set the port for the DNS Server interface to listen to (Default: 3444).\n"
" -di, --dns-ifce	Set the interface for the DNS Server interface to listen to (Default: <any>).\n"
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
#ifdef TUMBLEWEED
	char *p0 = NULL, *p1 = NULL;

	/* WWW Directory */
	if( strcmp( var, "-s") == 0 && val != NULL && strlen( val ) > 1 ) {
		p0 = val;

		if( *p0 == '/' ) {
			/* Absolute path? */
			snprintf( _main->conf->home, MAIN_BUF+1, "%s", p0 );
		} else {
			/* Relative path? */
			if( ( p1 = getenv( "PWD")) != NULL ) {
				snprintf( _main->conf->home, MAIN_BUF+1, "%s/%s", p1, p0 );
			} else {
				snprintf( _main->conf->home, MAIN_BUF+1, "/notexistant" );
			}
		}
	}

	/* Create HTML index */
	if( strcmp( var, "-i") == 0 && val != NULL && strlen( val ) > 1 ) {
		snprintf( _main->conf->index_name, MAIN_BUF+1, "%s", val );
	}

	/* IPv6 only */
	if( strcmp( var, "-6") == 0 && val == NULL ) {
		_main->conf->ipv6_only = TRUE;
	}

#endif

#ifdef MASALA
	if( match( var, "-ba", "--bootstrap-addr") ) {
		replace( var, &_main->conf->bootstrap_node, val);
	} else if( match( var, "-bp", "--bootstrap-port" ) ) {
		replace( var, &_main->conf->bootstrap_port, val);
	} else if( match( var, "-k", "--key" ) ) {
		replace( var, &_main->conf->key, val);
		_main->conf->bool_encryption = TRUE;
	} else if( match( var, "-h", "--hostname" ) ) {
		replace( var, &_main->conf->hostname, val);

		/* Compute host_id. Respect the realm. */
		p2p_compute_realm_id( _main->conf->host_id, _main->conf->hostname );

	} else if( match( var, "-r", "-realm" ) ) {
		replace( var, &_main->conf->realm, val);
		_main->conf->bool_realm = TRUE;

		/* Change realm. Recompute the host_id. */
		p2p_compute_realm_id( _main->conf->host_id, _main->conf->hostname );

	} else if( match( var, "-q", "--quiet" ) ) {
		if( val != NULL )
			no_arg_expected( var );
		_main->conf->quiet = CONF_BEQUIET;
	} else if( match( var, "-dp", "--dns-port" ) ) {
		replace( var, &_main->conf->dns_port, val);
	} else if( match( var, "-da", "--dns-addr" ) ) {
		replace( var, &_main->conf->dns_addr, val);
	} else if( match( var, "-di", "--dns-ifce" ) ) {
		replace( var, &_main->conf->dns_ifce, val);
	} else if( match( var, "-p", "--port" ) ) {
		if( val == NULL )
			arg_expected( var );
		_main->conf->port = atoi( val );
	} else if( match( var, "-u", "--username" ) ) {
		replace( var, &_main->conf->username, val);
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
#endif
}
