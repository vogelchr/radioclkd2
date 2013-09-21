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

//NOTE: this code HAS *NOT* been tested at all

#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "systime.h"


#include "clock.h"
#include "decode_dcf77.h"

#include "logger.h"
#include "settings.h"
#include "utctime.h"


#define	CENTURY		(2000)	//added to 2 digit years

//#define	WWVB_OFFSET_SEC	((time_f)0.019)	//approx delay for WWVB reciever to trigger a pulse at the start of a second
//this can be further fine-tuned with fudge time1 in ntp.conf, if required

//all these macros assume the following:
//1. clock is a variable for the current clock
//2. the index used is the second index
#define	DATA_OK(bit)	((clock->numdata-60+(bit)) >= 0 && (clock->numdata-60+(bit)) < clock->numdata)
#define	GET_DATA(bit)	(DATA_OK(bit) ? clock->data[clock->numdata-60+(bit)] : 0)
#define	GET(bit)	(GET_DATA(bit)==5)


int
wwvbGetBCD ( clkInfoT* clock, int valstart, int valcount )
{
	static const int BCD[12] = { 200, 100, 0, 80, 40, 20, 10, 0, 8, 4, 2, 1 };
	int	val,i;

	if ( valcount > 12 )
		return -1;

	val = 0;

	for ( i=0; i<valcount; i++ )
	{
		if ( GET(valstart+i) )
			val += BCD [ 12 - valcount + i ];
	}

	return val;
}

void
wwvbDump ( clkInfoT* clock )
{
	int	i;

	loggerf ( LOGGER_TRACE, "WWVB : " );
	for ( i=0; i<60; i+=5 )
		loggerf ( LOGGER_TRACE, "|%-4d", i );
	loggerf ( LOGGER_TRACE, "\n" );

	loggerf ( LOGGER_TRACE, "WWVB : " );
	for ( i=0; i<60; i++ )
		loggerf ( LOGGER_TRACE, "%c", DATA_OK(i)?(GET(i)?'1':'.'):' ' );
	loggerf ( LOGGER_TRACE, "\n" );
}


int
wwvbDecode ( clkInfoT* clock, time_f minstart )
{
	struct tm	dectime;
	time_t		dectimet;

	wwvbDump ( clock );

	if ( !DATA_OK(1) )
		return -1;

	memset ( &dectime, 0, sizeof(dectime) );

	dectime.tm_year = wwvbGetBCD ( clock, 44, 10 ) + CENTURY - 1900;
	dectime.tm_mon = 1;
	dectime.tm_mday = wwvbGetBCD ( clock, 22, 12 );
	dectime.tm_hour = wwvbGetBCD ( clock, 12, 7 );
	dectime.tm_min = wwvbGetBCD ( clock, 1, 8 );
	dectime.tm_sec = 0;
	dectime.tm_isdst = 0;	//time is always UTC... (although there are bits for summer time)

	//NOTE: decoding this depends on a mktime() which will normalize day numbers correctly

	if ( (dectime.tm_mday < 1) || (dectime.tm_mday > 366) )
		return -1;
	if ( dectime.tm_hour > 23 )
		return -1;
	if ( dectime.tm_min > 60 )
		return -1;

	loggerf ( LOGGER_DEBUG, "WWVB time: %04d-%03d %02d:%02d %s%s\n", dectime.tm_year+1900, dectime.tm_mday, dectime.tm_hour, dectime.tm_min, GET(55)?" leap year":"", GET(56)?" leap second soon":"" );

//	setenv ( "TZ", "", 1 );
//
//	dectimet = mktime ( &dectime );
	dectimet = UTCtime ( &dectime );
	if ( dectimet == (time_t)(-1) )
		return -1;

	clock->pctime = minstart;
	clock->radiotime = dectimet + clock->fudgeoffset;
	clock->radioleap = GET(56) ? LEAP_ADDSECOND : LEAP_NOWARNING;

	clock->secondssincetime = 0;

	return 0;
}
