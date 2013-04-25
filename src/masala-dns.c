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
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <netdb.h>
#include <sys/epoll.h>

#include "malloc.h"
#include "thrd.h"
#include "main.h"
#include "str.h"
#include "list.h"
#include "hash.h"
#include "log.h"
#include "conf.h"
#include "file.h"
#include "unix.h"
#include "opts.h"
#include "udp.h"
#include "ben.h"
#include "aes.h"
#include "node_p2p.h"
#include "bucket.h"
#include "lookup.h"
#include "announce.h"
#include "neighboorhood.h"
#include "p2p.h"
#include "send_p2p.h"
#include "search.h"
#include "cache.h"
#include "time.h"
#include "random.h"
#include "sha1.h"
#include "database.h"
#include "masala-dns.h"


/*
* DNS-Server interface for Masala.
*/

static const uint QR_MASK = 0x8000;
static const uint OPCODE_MASK = 0x7800;
static const uint AA_MASK = 0x0400;
static const uint TC_MASK = 0x0200;
static const uint RD_MASK = 0x0100;
static const uint RA_MASK = 0x8000;
static const uint RCODE_MASK = 0x000F;


/* Response Type */
enum {
	Ok_ResponseType = 0,
	FormatError_ResponseType = 1,
	ServerFailure_ResponseType = 2,
	NameError_ResponseType = 3,
	NotImplemented_ResponseType = 4,
	Refused_ResponseType = 5
};

/* Resource Record Types */
enum {
	A_Resource_RecordType = 1,
	NS_Resource_RecordType = 2,
	CNAME_Resource_RecordType = 5,
	SOA_Resource_RecordType = 6,
	PTR_Resource_RecordType = 12,
	MX_Resource_RecordType = 15,
	TXT_Resource_RecordType = 16,
	AAAA_Resource_RecordType = 28
};

/* Operation Code */
enum {
	QUERY_OperationCode = 0, /* standard query */
	IQUERY_OperationCode = 1, /* inverse query */
	STATUS_OperationCode = 2, /* server status request */
	NOTIFY_OperationCode = 4, /* request zone transfer */
	UPDATE_OperationCode = 5 /* change resource records */
};

/* Response Code */
enum {
	NoError_ResponseCode = 0,
	FormatError_ResponseCode = 1,
	ServerFailure_ResponseCode = 2,
	NameError_ResponseCode = 3
};

/* Query Type */
enum {
	IXFR_QueryType = 251,
	AXFR_QueryType = 252,
	MAILB_QueryType = 253,
	MAILA_QueryType = 254,
	STAR_QueryType = 255
};

/* Question Section */
struct question
{
	char *qName;
	uint qType;
	uint qClass;
};

union resource_data
{
	struct { char *txt_data; } txt_record;
	struct { UCHAR addr[4]; } a_record;
	struct { char *name; } name_server_record;
	struct { char *name; } cname_record;
	struct { char *name; } ptr_record;
	struct { uint preference; char *exchange; } mx_record;
	struct { UCHAR addr[16]; } aaaa_record;
};

/* Resource Record Section */
struct ResourceRecord
{
	char *name;
	uint type;
	uint class;
	uint ttl;
	uint rd_length;
	union resource_data rd_data;
};

struct message
{
	uint id; /* Identifier */

	/* flags */
	uint qr; /* Query/Response Flag */
	uint opcode; /* Operation Code */
	uint aa; /* Authoritative Answer Flag */
	uint tc; /* Truncation Flag */
	uint rd; /* Recursion Desired */
	uint ra; /* Recursion Available */
	uint rcode; /* Response Code */

	uint qdCount; /* Question Count */
	uint anCount; /* Answer Record Count */
	uint nsCount; /* Authority Record Count */
	uint arCount; /* Additional Record Count */

	/* We only handle one question and one answer! */
	struct question question;
	struct ResourceRecord answer;
};


/*
* Basic memory operations.
*/

int get16bits(const UCHAR** buffer)
{
	int value = (*buffer)[0];
	value = value << 8;
	value += (*buffer)[1];
	(*buffer) += 2;
	return value;
}

void put16bits(UCHAR** buffer, uint value)
{
	(*buffer)[0] = (value & 0xFF00) >> 8;
	(*buffer)[1] = value & 0xFF;
	(*buffer) += 2;
}

void put32bits(UCHAR** buffer, ulong value)
{
	(*buffer)[0] = (value & 0xFF000000) >> 24;
	(*buffer)[1] = (value & 0xFF0000) >> 16;
	(*buffer)[2] = (value & 0xFF00) >> 16;
	(*buffer)[3] = (value & 0xFF) >> 16;
	(*buffer) += 4;
}

/*
* Deconding/Encoding functions.
*/

/* 3foo3bar3com0 => foo.bar.com */
char *decode_domain_name( const UCHAR** buffer )
{
	UCHAR name[256];
	const UCHAR *buf = *buffer;
	char *dup;
	int j = 0;
	int i = 0;
	int len = 0;

	while(buf[i] != 0 && i < sizeof(name))
	{
		if(i != 0)
		{
			name[j] = '.';
			j += 1;
		}

		len = buf[i];
		i += 1;

		memcpy(name+j, buf+i, len);
		i += len;
		j += len;
	}

	name[j] = '\0';

	*buffer += i + 1; /* also jump over the last 0 */

	dup = (char*) malloc(j+1);
	if(dup)
	{
		memcpy(dup, name, j+1);
		return dup;
	}
	else
	{
		return NULL;
	}
}

/* foo.bar.com => 3foo3bar3com0 */
void code_domain_name( UCHAR** buffer, const char *domain )
{
	char *buf = (char*) *buffer;
	const char *beg = domain;
	const char *pos;
	int len = 0;
	int i = 0;

	while((pos = strchr(beg, '.')) != '\0')
	{
		len = pos - beg;
		buf[i] = len;
		i += 1;
		memcpy(buf+i, beg, len);
		i += len;

		beg = pos + 1;
	}

	len = strlen(domain) - (beg - domain);

	buf[i] = len;
	i += 1;

	memcpy(buf + i, beg, len);
	i += len;

	buf[i] = 0;
	i += 1;

	*buffer += i;
}

void dns_decode_header( struct message *msg, const UCHAR** buffer )
{
	uint fields;

	msg->id = get16bits(buffer);
	fields = get16bits(buffer);
	msg->qr = fields & QR_MASK;
	msg->opcode = fields & OPCODE_MASK;
	msg->aa = fields & AA_MASK;
	msg->tc = fields & TC_MASK;
	msg->rd = fields & RD_MASK;
	msg->ra = fields & RA_MASK;
	msg->rcode = fields & RCODE_MASK;


	msg->qdCount = get16bits(buffer);
	msg->anCount = get16bits(buffer);
	msg->nsCount = get16bits(buffer);
	msg->arCount = get16bits(buffer);
}

void dns_code_header(struct message *msg, UCHAR** buffer)
{
	uint fields;

	put16bits(buffer, msg->id);
	fields = (msg->qr << 15);
	fields += (msg->opcode << 14);
	/* TODO: insert the rest of the field */
	fields += msg->rcode;
	put16bits( buffer, fields);

	put16bits( buffer, msg->qdCount );
	put16bits( buffer, msg->anCount );
	put16bits( buffer, msg->nsCount );
	put16bits( buffer, msg->arCount );
}

int dns_decode_query( struct message *msg, const UCHAR *buffer, int size )
{
	int i;

	dns_decode_header( msg, &buffer );

	if(( msg->anCount+msg->nsCount+msg->arCount) != 0 )
	{
		log_info("DNS: Only questions expected.");
		return -1;
	}

	/* parse questions */
	for(i = 0; i < msg->qdCount; ++i)
	{
		char *qName = decode_domain_name( &buffer );
		int qType = get16bits( &buffer );
		int qClass = get16bits( &buffer );

		if( qType == AAAA_Resource_RecordType )
		{
			msg->question.qName = qName;
			msg->question.qType = qType;
			msg->question.qClass = qClass;
			return 1;
		}
		else
		{
			myfree( qName, "dns_decode_query" );
		}
	}

	return -1;
}

UCHAR * dns_code_response( struct message *msg, UCHAR *buffer )
{
	dns_code_header( msg, &buffer );
	
	if(msg->anCount == 1)
	{
		/* Attach a single question section. */
		code_domain_name( &buffer, msg->question.qName );
		put16bits( &buffer, msg->question.qType );
		put16bits( &buffer, msg->question.qClass );

		/* Attach a single resource section. */
		code_domain_name( &buffer, msg->answer.name );
		put16bits( &buffer, msg->answer.type );
		put16bits( &buffer, msg->answer.class );
		put32bits( &buffer, msg->answer.ttl );
		put16bits( &buffer, msg->answer.rd_length );

		memcpy( buffer, &msg->answer.rd_data.aaaa_record.addr, 16 );
		buffer += 16;
	}
	return buffer;
}


void dns_send_response( int sockfd, struct message *msg, IP *from, IP *record ) {
	UCHAR buf[1500];
	struct ResourceRecord *a;
	struct question *q;
	char addrbuf1[FULL_ADDSTRLEN];
	char addrbuf2[FULL_ADDSTRLEN];

	a = &msg->answer;
	q = &msg->question;

	/* Header: leave most values intact for response */
	msg->qr = 1; /* this is a response */
	msg->aa = 1; /* this server is authoritative */
	msg->ra = 0; /* no recursion available - we don't ask other dns servers */
	msg->rcode = Ok_ResponseType;
	msg->anCount = 1;
	msg->nsCount = 0;
	msg->arCount = 0;

	/* Set AAAA Resource Record */
	a->name = q->qName;
	a->type = q->qType;
	a->class = q->qClass;
	a->ttl = 0; /* no caching */
	a->rd_length = 16;
	memcpy( a->rd_data.aaaa_record.addr, &record->sin6_addr, 16 );

	UCHAR* p = dns_code_response( msg, buf );

	if( p )
	{
		int buflen = p - buf;
		log_debug( "DNS: send address [%s] to [%s]. Packet is %d Bytes.", addr_str(record, addrbuf1 ), addr_str(from, addrbuf2 ), buflen);

		sendto( sockfd, buf, buflen, 0, (struct sockaddr*) from, sizeof(IP) );
	}
}


int dns_masala_lookup(const char *hostname, size_t size, IP *from, IP *record) {
	UCHAR lkp_id[SHA_DIGEST_LENGTH];
	UCHAR host_id[SHA_DIGEST_LENGTH];
	IP *addr;
	char hexbuf[HEX_LEN+1];

	/* Validate hostname */
	if ( !str_isValidHostname( (char *)hostname, size ) ) {
		log_info( "DNS: Invalid hostname for lookup: '%s'", hostname );
		return -1;
	}

	/* That is the lookup key */
	p2p_compute_realm_id( host_id, (char *)hostname );
	log_debug( "DNS: Lookup %s as '%s'.", hostname, id_str( host_id, hexbuf ) );

	/* Check my own DB for that node. */
	mutex_block( _main->p2p->mutex );
	addr = db_address( host_id );
	mutex_unblock( _main->p2p->mutex );

	if( addr != NULL ) {
		log_info( "DNS: Found entry for '%s'.", hostname );
		memcpy( &record->sin6_addr, &addr->sin6_addr, 16 );
		return 1;
	}

	log_info( "DNS: No local entry found. Create P2P request for '%s'.", hostname  );

	/* Create random id to identify this search request */
	rand_urandom( lkp_id, SHA_DIGEST_LENGTH );

	/* Start find process */
	mutex_block( _main->p2p->mutex );
	lkp_put( host_id, lkp_id, from );
	mutex_unblock( _main->p2p->mutex );
	return 0;
}

void* dns_loop( void* _ ) {

	char *bind_addr = _main->conf->dns_addr;
	char *bind_ifce = _main->conf->dns_ifce;
	char *bind_port = _main->conf->dns_port;
	
	int rc;
	int val;
	struct addrinfo hints, *servinfo, *p;
	struct timeval tv;

	int sockfd;
	IP from, record, sockaddr;
	UCHAR buffer[1500];
	socklen_t addr_len = sizeof(IP);
	struct message msg;
	char addrbuf[FULL_ADDSTRLEN];


	memset( &hints, 0, sizeof(hints) );
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;

	if( (rc = getaddrinfo( bind_addr, bind_port, &hints, &servinfo )) == 0 ) {
		for( p = servinfo; p != NULL; p = p->ai_next ) {
			memset( &sockaddr, 0, sizeof(IP) );
			sockaddr = *((IP*) p->ai_addr);
			freeaddrinfo(servinfo);
			break;
		}
    } else {
		log_err( "DNS getaddrinfo failed: %s", gai_strerror(rc));
        return NULL;
	}

	sockfd = socket( PF_INET6, SOCK_DGRAM, IPPROTO_UDP );
	if(sockfd < 0) {
		log_err( "DNS: Failed to create socket: %s", gai_strerror(errno) );
		return NULL;
	}

	if( bind_ifce && setsockopt( sockfd, SOL_SOCKET, SO_BINDTODEVICE, bind_ifce, strlen( bind_ifce )) ) {
		log_err( "DNS: Unable to bind to device '%s': %s", bind_ifce,  gai_strerror(errno ) );
		return NULL;
	}

	val = 1;
	rc = setsockopt( sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &val, sizeof(val) );
	if(rc < 0) {
		log_err( "DNS: Failed to set socket option IPV6_V6ONLY." );
		return NULL;
	}

	rc = bind( sockfd, (struct sockaddr*) &sockaddr, sizeof(IP) );
	if(rc < 0) {
		log_err( "DNS: Failed to bind socket to address: %s", gai_strerror(rc) );
		return NULL;
	}

	/* Set receive timeout */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv) );

	log_info( "Bind DNS interface to %s, device %s.",
		bind_addr ? addr_str( &sockaddr, addrbuf ) : "<any>",
		bind_ifce ? bind_ifce : "<any>"
	);

	while(1) {
		memset( &msg, 0, sizeof(msg) );
		rc = recvfrom( sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &from, &addr_len );

		if(rc < 0)
			continue;

		log_debug( "DNS: Received query from %s.",  addr_str( &from, addrbuf )  );

		rc = dns_decode_query( &msg, buffer, rc );
		if(rc < 0)
			continue;

		rc = dns_masala_lookup( msg.question.qName, strlen(msg.question.qName), &from, &record );

		if(rc > 0) {
			log_debug("DNS: Lookup succeded for '%s'.", msg.question.qName );
			dns_send_response( sockfd, &msg, &from, &record );
		} else {
			log_debug("DNS: Lookup failed for '%s'.", msg.question.qName );
		}
	}

	return NULL;
}

int dns_start()
{
	pthread_t tid;
	
	int rc = pthread_create( &tid, NULL, &dns_loop, 0 );
	if (rc != 0) {
		log_crit( "DNS: Failed to create a new thread." );
		return 1;
	}

	pthread_detach( tid );
	
	return 0;
}
