/*
 * Copyright (c) 2002 Jon Atkins http://www.jonatkins.com/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>



static FILE*	logf_file;
static int	logf_file_level;

static int	logf_syslog = 0;
static int	logf_syslog_level;

void
loggerSetFile ( FILE* file, int level )
{
	logf_file = file;
	logf_file_level = level;
}

void
loggerSyslog ( int flag, int level )
{
	logf_syslog = flag;
	logf_syslog_level = level;
}

void
loggerf ( int level, char* format, ... )
{
	char	buf[512];
	va_list	ap;
	static char	syslogline[512];


	if ( format )
	{
		va_start ( ap, format );
		vsnprintf ( buf, sizeof(buf), format, ap );
		va_end ( ap );

		if ( logf_file && level <= logf_file_level )
		{
			fprintf ( logf_file, "%s", buf );
			fflush ( logf_file );
		}

		if ( logf_syslog && level <= logf_syslog_level )
		{
			//some (all?) syslog()s output strings without '\n' in them as a line anyway - so buffer it here...

			//if we have a lot, send it out anyway...
			if ( strlen(syslogline)+strlen(buf) >= sizeof(syslogline) )
			{
				syslog ( LOG_NOTICE, "%s", syslogline );
				syslogline[0] = 0;
			}

			strcat ( syslogline, buf );

			if ( syslogline[0] != 0 && syslogline[strlen(syslogline)-1] == '\n' )
			{
				syslog ( LOG_NOTICE, "%s", syslogline );
				syslogline[0] = 0;
			}

		}
	}
}
