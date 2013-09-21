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

//#define	DCF77_OFFSET_SEC	((time_f)0.020)	//approx delay for DCF77 reciever to trigger a pulse at the start of a second
//this can be further fine-tuned with fudge time1 in ntp.conf, if required

//all these macros assume the following:
//1. clock is a variable for the current clock
//2. the index used is the second index
#define	DATA_OK(bit)	((clock->numdata-60+(bit)) >= 0 && (clock->numdata-60+(bit)) < clock->numdata)
#define	GET_DATA(bit)	(DATA_OK(bit) ? clock->data[clock->numdata-60+(bit)] : 0)
#define	GET(bit)	(GET_DATA(bit)==2)


int
dcf77CheckParity ( clkInfoT* clock, int valstart, int valcount, int paritypos )
{
	int	parity, i;

	parity = 0;

	for ( i=0; i<valcount; i++ )
		parity ^= GET(valstart+i);

	parity ^= GET(paritypos);

	return parity;
}

int
dcf77GetBCD ( clkInfoT* clock, int valstart, int valcount )
{
	static const int BCD[8] = { 1, 2, 4, 8, 10, 20, 40, 80 };
	int	val, i;

	if ( valcount > 8 )
		return -1;

	val = 0;

	for ( i=0; i<valcount; i++ )
	{
		if ( GET(valstart+i) )
			val += BCD [ i ];
	}

	return val;
}

void
dcf77Dump ( clkInfoT* clock )
{
	int	i;

	loggerf ( LOGGER_TRACE, "DCF77: " );
	for ( i=0; i<60; i+=5 )
		loggerf ( LOGGER_TRACE, "|%-4d", i );
	loggerf ( LOGGER_TRACE, "\n" );

	loggerf ( LOGGER_TRACE, "DCF77: " );
	for ( i=0; i<60; i++ )
		loggerf ( LOGGER_TRACE, "%c", DATA_OK(i)?(GET(i)?'1':'.'):' ' );
	loggerf ( LOGGER_TRACE, "\n" );
}

int
dcf77Decode ( clkInfoT* clock, time_f minstart )
{
	struct tm	dectime;
	time_t		dectimet;

	dcf77Dump ( clock );

	if ( !DATA_OK(15) )
		return -1;


	if ( !GET(20) )	//start bit
		return -1;


	if ( dcf77CheckParity ( clock, 21, 7, 28 ) )	//minutes parity
		return -1;

	if ( dcf77CheckParity ( clock, 29, 6, 35 ) )	//hours parity
		return -1;

	if ( dcf77CheckParity ( clock, 36, 22, 58 ) )	//day/dow/month/year parity
		return -1;


	if ( !(GET(17) ^ GET(18)) )	//only one of Z1/Z2 should be set
		return -1;

	memset ( &dectime, 0, sizeof(dectime) );


	dectime.tm_year = dcf77GetBCD ( clock, 50, 8 ) + CENTURY - 1900;
	dectime.tm_mon = dcf77GetBCD ( clock, 45, 5 ) - 1;
	dectime.tm_mday = dcf77GetBCD ( clock, 36, 6 );
	dectime.tm_wday = dcf77GetBCD ( clock, 42, 3 );
	dectime.tm_hour = dcf77GetBCD ( clock, 29, 6 );
	dectime.tm_min = dcf77GetBCD ( clock, 21, 7 );
	dectime.tm_sec = 0;
	dectime.tm_isdst = 0;	//decode as UTC, then correct for CET/CEST later

	if ( dectime.tm_wday == 7 )
		dectime.tm_wday = 0;

	if ( dectime.tm_mon > 11 )
		return -1;
	if ( (dectime.tm_mday < 1) || (dectime.tm_mday > 31) )
		return -1;
	if ( dectime.tm_wday > 6 )
		return -1;
	if ( dectime.tm_hour > 23 )
		return -1;
	if ( dectime.tm_min > 60 )
		return -1;

	loggerf ( LOGGER_DEBUG, "DCF77 time: %04d-%02d-%02d (day %d) %02d:%02d %s%s%s%s\n", dectime.tm_year+1900, dectime.tm_mon, dectime.tm_mday, dectime.tm_wday, dectime.tm_hour, dectime.tm_min, GET(17) ? "CEST" : "CET", GET(16) ? " timezone change soon":"", GET(19) ? " leap second soon":"", GET(15) ? " res ant" : " main ant" );


//	setenv("TZ", "", 1);
//
//	dectimet = mktime ( &dectime );
	dectimet = UTCtime ( &dectime );

	if ( dectimet == (time_t)(-1) )
		return -1;

	//correct for DST offset...
	dectimet -= GET(17) ? 2*60*60 : 1*60*60;

	//right - the time seems OK now...

	clock->pctime = minstart;
	clock->radiotime = dectimet + clock->fudgeoffset;
	clock->radioleap = GET(19) ? LEAP_ADDSECOND : LEAP_NOWARNING;

	clock->secondssincetime = 0;


	return 0;
}
