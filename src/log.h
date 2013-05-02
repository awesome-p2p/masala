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

#ifdef TUMBLEWEED
void log_complex( NODE *n, int code, const char *buffer );
#endif

#ifdef TUMBLEWEED
void log_info( int code, const char *buffer );
#endif

#include <syslog.h>

#define log_crit(...) _log(NULL, 0, LOG_CRIT, __VA_ARGS__)
#define log_err(...) _log(NULL, 0, LOG_ERR, __VA_ARGS__)
#define log_info(...) _log(NULL, 0, LOG_INFO, __VA_ARGS__)
#define log_warn(...) _log(NULL, 0, LOG_WARNING, __VA_ARGS__)

#define HEX_LEN (2 * SHA_DIGEST_LENGTH)
#define FULL_ADDSTRLEN (INET6_ADDRSTRLEN + 9)


char* id_str( const UCHAR *in, char *buf );
char* addr_str( IP *addr, char *buf );
void _log( const char *filename, int line, int priority, const char *format, ... );
