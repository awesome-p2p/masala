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
#include <netinet/in.h>
#include <signal.h>

#include "main.h"
#include "str.h"
#include "malloc.h"
#include "list.h"
#include "log.h"
#include "file.h"
#include "conf.h"
#include "unix.h"
#include "random.h"
#include "ben.h"
#include "p2p.h"

struct obj_conf *conf_init( void ) {
	struct obj_conf *conf = (struct obj_conf *) myalloc( sizeof(struct obj_conf), "conf_init" );

	conf->mode = CONF_FOREGROUND;
	conf->port = strdup( CONF_PORT );

	rand_urandom( conf->node_id, SHA_DIGEST_LENGTH );
	memset( conf->null_id, '\0', SHA_DIGEST_LENGTH );

	conf->cores = (unix_cpus() > 2) ? unix_cpus() : CONF_CORES;
	conf->quiet = CONF_VERBOSE;
	conf->user = strdup( CONF_USER );

	conf->bootstrap_node = strdup( CONF_BOOTSTRAP_NODE );
	conf->bootstrap_port = strdup( CONF_BOOTSTRAP_PORT );

#ifdef DNS
	conf->dns_port = strdup( CONF_DNS_PORT );
	conf->dns_addr = strdup( CONF_DNS_ADDR );
#endif
#ifdef WEB
	conf->web_port = strdup( CONF_WEB_PORT );
	conf->web_addr = strdup( CONF_WEB_ADDR );
#endif

	return conf;
}

void conf_free( void ) {
	if( _main->conf != NULL ) {
		myfree( _main->conf->user, "conf_free" );
		myfree( _main->conf->pid_file, "conf_free" );
		myfree( _main->conf->hostname, "conf_free" );
		myfree( _main->conf->bootstrap_node, "conf_free" );
		myfree( _main->conf->bootstrap_port, "conf_free" );
#ifdef DNS
		myfree( _main->conf->dns_port, "conf_free" );
		myfree( _main->conf->dns_addr, "conf_free" );
		myfree( _main->conf->dns_ifce, "conf_free" );
#endif
#ifdef WEB
		myfree( _main->conf->web_port, "conf_free" );
		myfree( _main->conf->web_addr, "conf_free" );
		myfree( _main->conf->web_ifce, "conf_free" );
#endif
		myfree( _main->conf->port, "conf_free" );
		myfree( _main->conf->interface, "conf_free" );
		myfree( _main->conf, "conf_free" );
	}
}

void conf_check( void ) {
	char hexbuf[HEX_LEN+1];

	if( _main->conf->hostname != NULL ) {
		log_info( "Hostname: '%s' (-h)",  _main->conf->hostname );
		log_info( "Host ID: %s", id_str(  _main->conf->host_id, hexbuf ), _main->conf->hostname );
	} else {
		log_info( "Hostname: <none> (-h)" );
	}
	log_info( "Node ID: %s", id_str( _main->conf->node_id, hexbuf ) );
	log_info( "Bootstrap Node: %s (-ba)", _main->conf->bootstrap_node );
	log_info( "Bootstrap Port: UDP/%s (-bp)", _main->conf->bootstrap_port );

	if( _main->conf->mode == CONF_FOREGROUND ) {
		log_info( "Mode: Foreground (-d)" );
	} else {
		log_info( "Mode: Daemon (-d)" );
	}

	if( _main->conf->quiet == CONF_BEQUIET ) {
		log_info( "Verbosity: Quiet (-q)" );
	} else {
		log_info( "Verbosity: Verbose (-q)" );
	}

	if( _main->conf->interface ) {
		log_info( "Listen to UDP/%s (-p), interface '%s' (-i)",  _main->conf->port, _main->conf->interface );
	} else {
		log_info( "Listen to UDP/%s (-p), interface <any> (-i)", _main->conf->port, _main->conf->interface );
	}

	/* Port == 0 => Random source port */
	if( str_isSafePort( _main->conf->port ) < 0 ) {
		log_err( "Invalid www port number. (-p)" );
	}

	/* Check bootstrap server port */
	if( str_isSafePort( _main->conf->bootstrap_port ) < 0 ) {
		log_err( "Invalid bootstrap port number. (-bp)" );
	}

	log_info( "Worker threads: %i", _main->conf->cores );
	if( _main->conf->cores < 1 || _main->conf->cores > 128 ) {
		log_err( "Invalid core number." );
	}
}
