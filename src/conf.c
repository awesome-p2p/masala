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
#include <netinet/in.h>
#include <signal.h>

#ifdef TUMBLEWEED
#include "main.h"
#include "str.h"
#include "malloc.h"
#include "list.h"
#include "node_web.h"
#include "log.h"
#include "file.h"
#include "conf.h"
#include "unix.h"
#else
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
#endif

struct obj_conf *conf_init( void ) {
	struct obj_conf *conf = (struct obj_conf *) myalloc( sizeof(struct obj_conf), "conf_init" );

	conf->mode = CONF_FOREGROUND;
	conf->port = strdup( CONF_PORT );

#ifdef TUMBLEWEED
	if( ( getenv( "HOME")) == NULL ) {
		strncpy( conf->home, "/var/www", MAIN_BUF );
	} else {
		snprintf( conf->home, MAIN_BUF+1, "%s/%s", getenv( "HOME"), "Public" );
	}
#endif

#ifdef MASALA
	rand_urandom( conf->node_id, SHA_DIGEST_LENGTH );
#endif

#ifdef MASALA
	memset( conf->null_id, '\0', SHA_DIGEST_LENGTH );
#endif

	conf->cores = (unix_cpus() > 2) ? unix_cpus() : CONF_CORES;
	conf->quiet = CONF_VERBOSE;
	conf->user = strdup( CONF_USER );

#ifdef MASALA
	conf->bootstrap_node = strdup( CONF_BOOTSTRAP_NODE );
	conf->bootstrap_port = strdup( CONF_BOOTSTRAP_PORT );
#endif

#ifdef TUMBLEWEED
	snprintf( conf->index_name, MAIN_BUF+1, "%s", CONF_INDEX_NAME );
#endif

#ifdef TUMBLEWEED
	conf->ipv6_only = FALSE;
#endif

#ifdef MASALA
	conf->dns_port = strdup( CONF_DNS_PORT );
	conf->dns_addr = strdup( CONF_DNS_ADDR );
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
		myfree( _main->conf->dns_port, "conf_free" );
		myfree( _main->conf->dns_addr, "conf_free" );
		myfree( _main->conf->dns_ifce, "conf_free" );
		myfree( _main->conf->port, "conf_free" );
		myfree( _main->conf->interface, "conf_free" );
		myfree( _main->conf, "conf_free" );
	}
}

void conf_check( void ) {
	char hexbuf[HEX_LEN+1];
#ifdef MASALA
	if( _main->conf->hostname != NULL ) {
		log_info( "Hostname: '%s' (-h)",  _main->conf->hostname );
		log_info( "Host ID: %s", id_str(  _main->conf->host_id, hexbuf ), _main->conf->hostname );
	} else {
		log_info( "Hostname: <none> (-h)" );
	}
	log_info( "Node ID: %s", id_str( _main->conf->node_id, hexbuf ) );
	log_info( "Bootstrap Node: %s (-ba)", _main->conf->bootstrap_node );
	log_info( "Bootstrap Port: UDP/%s (-bp)", _main->conf->bootstrap_port );
#endif

#ifdef TUMBLEWEED
	log_info( "Shared: %s (-s)", _main->conf->home );
	
	if( !file_isdir( _main->conf->home) ) {
		log_err( "The shared directory does not exist" );
	}
#endif

#ifdef TUMBLEWEED
	log_info( "Index file: %s (-i)", _main->conf->index_name );
	if( !str_isValidFilename( _main->conf->index_name ) ) {
		log_err( "%s looks suspicious", _main->conf->index_name );
	}
#endif

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

#ifdef TUMBLEWEED
	log_info( "Listen to TCP/%s (-p)", _main->conf->port );
#elif MASALA
	if( _main->conf->interface ) {
		log_info( "Listen to UDP/%s (-p), interface '%s' (-i)",  _main->conf->port, _main->conf->interface );
	} else {
		log_info( "Listen to UDP/%s (-p), interface <any> (-i)", _main->conf->port, _main->conf->interface );
	}
#endif

	/* Port == 0 => Random source port */
	if( str_isSafePort( _main->conf->port ) < 0 ) {
		log_err( "Invalid www port number. (-p)" );
	}

	/* Check bootstrap server port */
#ifndef TUMBLEWEED
	if( str_isSafePort( _main->conf->bootstrap_port ) < 0 ) {
		log_err( "Invalid bootstrap port number. (-bp)" );
	}
#endif

	log_info( "Worker threads: %i", _main->conf->cores );
	if( _main->conf->cores < 1 || _main->conf->cores > 128 ) {
		log_err( "Invalid core number." );
	}
}
