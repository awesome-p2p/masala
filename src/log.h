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
#elif MASALA
void log_complex( IP *c_addr, const char *buffer );
#endif

#ifdef TUMBLEWEED
void log_info( int code, const char *buffer );
#elif MASALA
void log_info( const char *buffer );
#endif

#include <syslog.h>

#ifdef DEBUG
#define log_crit(...) __log(__FILE__, __LINE__, LOG_CRIT, __VA_ARGS__)
#define log_err(...) __log(__FILE__, __LINE__, LOG_ERR, __VA_ARGS__)
#define log_info(...) __log(__FILE__, __LINE__, LOG_INFO, __VA_ARGS__)
#define log_debug(...) __log(__FILE__, __LINE__, LOG_DEBUG, __VA_ARGS__)
#else
#define log_crit(...) __log(NULL, 0, LOG_CRIT, __VA_ARGS__)
#define log_err(...) __log(NULL, 0, LOG_ERR, __VA_ARGS__)
#define log_info(...) __log(NULL, 0, LOG_INFO, __VA_ARGS__)
#define log_debug(...) __log(NULL, 0, LOG_DEBUG, __VA_ARGS__)
#endif

void __log(const char *filename, int line, int priority, const char *format, ...);
