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
#include <string.h>

#include "memory.h"
#include "logger.h"


void*
safe_mallocz ( size_t size )
{
	void*	ptr;

	ptr = malloc ( size );
	if ( ptr == NULL )
	{
		loggerf ( LOGGER_NOTE, "Error: malloc() failed\n" );
		exit(1);
	}
	memset ( ptr, 0, size );
	return ptr;
}

void
safe_free ( void* ptr )
{
	if ( ptr )
		free ( ptr );
}

char*
safe_xstrcpy ( char* str, int len )
{
	char*	ret;


	if ( len < 0 )
		len = strlen(str);

	ret = malloc ( len + 1 );
	if ( ret == NULL )
	{
		loggerf ( LOGGER_NOTE, "Error: malloc() failed in safe_xstrcpy()\n" );
		exit(1);
	}

	memcpy ( ret, str, len );
	ret[len] = 0;

	return ret;
}
