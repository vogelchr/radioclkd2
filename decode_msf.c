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
#include "decode_msf.h"
#include "utctime.h"

#include "logger.h"
#include "settings.h"


#define	CENTURY		(2000)	//added to 2 digit years

//#define	MSF_OFFSET_SEC	((time_f)0.019)	//approx delay for MSF reciever to trigger a pulse at the start of a second
//this can be further fine-tuned with fudge time1 in ntp.conf, if required


//all these macros assume the following:
//1. clock is a variable for the current clock
//2. the index used is that in ctm001v03.pdf docs - see http://www.npl.co.uk/npl/ctm/msf.html
#define	DATA_OK(bit)	((clock->numdata-60+(bit)) >= 0 && (clock->numdata-60+(bit)) < clock->numdata)
#define	GET_DATA(bit)	(DATA_OK(bit) ? clock->data[clock->numdata-60+(bit)] : 0)
#define	GET_A(bit)	((GET_DATA(bit)==2)||(GET_DATA(bit)==3))
#define	GET_B(bit)	((GET_DATA(bit)==3)||(GET_DATA(bit)==11))

//MSF data format: 
//  0-100ms - always low
//100-200ms - bit a
//200-300ms - bit b
//data[] contains the length of the lows from the start of the second, with
//a special case of 11 for bit a clear (high) and bit b set (low) (ie. a 2nd low state in 1 second)


//calculates the parity of a set of A bits and it's matching B bit
int
msfCheckParity ( clkInfoT* clock, int valstart, int valcount, int paritypos )
{
	int	parity, i;

	parity = 0;

	for ( i=0; i<valcount; i++ )
		parity ^= GET_A(valstart+i);

	parity ^= GET_B(paritypos);


	return parity;
}


//calculate the value of a BCD number in the A bits... (max 8 bits)
int
msfGetBCDA ( clkInfoT* clock, int valstart, int valcount )
{
	static const int BCD[8] = { 80, 40, 20, 10, 8, 4, 2, 1 };
	int	val, i;

	if ( valcount > 8 )
		return -1;

	val = 0;

	for ( i=0; i<valcount; i++ )
	{
		if ( GET_A(valstart+i) )
			val += BCD [ 8 - valcount + i ];
	}

	return val;
}

void
msfDump ( clkInfoT* clock )
{
	int	i;

	loggerf ( LOGGER_TRACE, "MSF  : " );
	for ( i=0; i<60; i+=5 )
		loggerf ( LOGGER_TRACE, "|%-4d", i );
	loggerf ( LOGGER_TRACE, "\n" );

	loggerf ( LOGGER_TRACE, "MSF-A: " );
	for ( i=0; i<60; i++ )
		loggerf ( LOGGER_TRACE, "%c", DATA_OK(i)?(GET_A(i)?'A':'.'):' ' );
	loggerf ( LOGGER_TRACE, "\n" );

	loggerf ( LOGGER_TRACE, "MSF-B: " );
	for ( i=0; i<60; i++ )
		loggerf ( LOGGER_TRACE, "%c", DATA_OK(i)?(GET_B(i)?'B':'.'):' ' );
	loggerf ( LOGGER_TRACE, "\n" );

}


int
msfDecode ( clkInfoT* clock, time_f minstart )
{
//	int	year, month, mday, wday, hour, min;
	struct tm	dectime;
	time_t		dectimet;

	msfDump ( clock );

	//make sure the earliest bit we look at is there.. (start of year)
	if ( !DATA_OK(17) )
		return -1;

	//first, check that the parity bits make sense...

	if ( !msfCheckParity ( clock, 17, 8, 54 ) )	//year...
		return -1;

	if ( !msfCheckParity ( clock, 25, 11, 55 ) )	//month/month day...
		return -1;

	if ( !msfCheckParity ( clock, 36, 3, 56 ) )	//day of week...
		return -1;

	if ( !msfCheckParity ( clock, 39, 13, 57 ) )	//hour/minute...
		return -1;

	memset ( &dectime, 0, sizeof(dectime) );

	dectime.tm_year = msfGetBCDA ( clock, 17, 8 ) + CENTURY - 1900;
	dectime.tm_mon = msfGetBCDA ( clock, 25, 5 ) - 1;
	dectime.tm_mday = msfGetBCDA ( clock, 30, 6 );
	dectime.tm_wday = msfGetBCDA ( clock, 36, 3 );
	dectime.tm_hour = msfGetBCDA ( clock, 39, 6 );
	dectime.tm_min = msfGetBCDA ( clock, 45, 7 );
	dectime.tm_sec = 0;
	dectime.tm_isdst = 0;	//decode as UTC, then correct for BST later...

	//now do some sanity checks on the decoded time values...
//TODO: check year value..
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

	loggerf ( LOGGER_DEBUG, "MSF time: %04d-%02d-%02d (day %d) %02d:%02d %s%s\n", dectime.tm_year+1900, dectime.tm_mon, dectime.tm_mday, dectime.tm_wday, dectime.tm_hour, dectime.tm_min, GET_B(58) ? "BST" : "GMT", GET_B(53) ? " timezone change soon":"" );

//	setenv("TZ", "", 1);
//
//	dectimet = mktime ( &dectime );
	dectimet = UTCtime ( &dectime );

	if ( dectimet == (time_t)(-1) )
		return -1;

	//correct for DST offset...
	dectimet -= GET_B(58) ? 1*60*60 : 0*60*60;

	//right - the time seems OK now...

	clock->pctime = minstart;
	clock->radiotime = dectimet + clock->fudgeoffset;
	clock->radioleap = LEAP_NOWARNING;

	clock->secondssincetime = 0;


	return 0;
}

