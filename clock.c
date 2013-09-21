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
#include <math.h>


#include "memory.h"

#include "clock.h"

#include "decode_msf.h"
#include "decode_dcf77.h"
#include "decode_wwvb.h"

#include "shm.h"
#include "logger.h"
#include "settings.h"


static clkInfoT* clkListHead;


void
clkDumpData ( const clkInfoT* clock )
{
	int	i;

	loggerf ( LOGGER_TRACE, "data(%d): ", clock->numdata );
	for ( i=0; i<clock->numdata; i++ )
	{
		loggerf ( LOGGER_TRACE, "%d,", clock->data[i] );
	}
	loggerf ( LOGGER_TRACE, "\n" );
}


clkInfoT*
clkCreate ( int inverted, int shmunit, time_f fudgeoffset )
{
	clkInfoT*	clkinfo;

	clkinfo = safe_mallocz ( sizeof(clkInfoT) );
	clkinfo->next = clkListHead;
	clkListHead = clkinfo;

	clkinfo->inverted = inverted;
	clkinfo->fudgeoffset = fudgeoffset;

	clkinfo->numdata = 0;

	if ( !debugLevel )
		clkinfo->shm = shmCreate ( shmunit );

	return clkinfo;
}

void
clkDataClear ( clkInfoT* clock )
{
	clock->numdata = 0;
}

int
clkPulseLength ( time_f timef )
{
	//pulse/clear lengths for each radio clock
	//MSF: 0.1, 0.2, 0.3, 0.5, 0.7, 0.8, 0.9
	//DCF77: 0.1, 0.2, 0.8, 0.9, 1.8, 1.9 (note: these last 2 are to handle the missing second 59)
	//WWVB: 0.2, 0.5, 0.8
	static const time_f lengths[] = { 0.1, 0.2, 0.3, 0.5, 0.7, 0.8, 0.9, 1.8, 1.9, -1 };

	int	i;

	//only detect short pulses...
	if ( timef > 2.0 )
		return -1;

	for ( i=0; lengths[i] > 0; i++ )
	{
		if ( timef > (lengths[i]-0.04) && timef < (lengths[i]+0.040) )
			return (int)(lengths[i] * 10 + 0.5);	//to convert to 10ths of a second
	}
	return -1;
}


void
clkProcessStatusChange ( clkInfoT* clock, int status, time_f timef )
{
	time_f diff;
	int	val;


	if ( clock->inverted )
		status = !status;

//	timersub ( tv, &clock->changetime, &diff );
	diff = timef - clock->changetime;

	if ( !clock->status && status )
	{
		val = clkPulseLength ( diff );


		if ( val < 0 )
		{
			loggerf ( LOGGER_TRACE, "warning: bad pulse length "TIMEF_FORMAT"\n", diff );

			clkDataClear ( clock );
		}
		else
		{
			//make sure we dont go off the end - unlikely but could happen with noise...
			if ( clock->numdata >= 120 )
				clkDataClear ( clock );

			if ( clock->msf_skip_b && (val == 1) )
			{
				clock->msf_skip_b = 0;
				if ( clock->numdata >= 1 )
					clock->data[clock->numdata-1] += 10;
			}
			else
			{
				if ( val == 5 )	//MSF minute marker...
				{
					clkDumpData ( clock );
					if ( msfDecode ( clock, clock->changetime ) < 0 )
						loggerf ( LOGGER_DEBUG, "warning: failed to decode MSF time\n" );
					else
						clkSendTime ( clock );


					clkDataClear ( clock );
				}
				else if ( val == 8 )	//WWVB minute marker...
				{
					clkDumpData ( clock );
					if ( wwvbDecode ( clock, clock->changetime ) < 0 )
						loggerf ( LOGGER_DEBUG, "warning: failed to decode WWVB time\n" );
					else
						clkSendTime ( clock );

					clkDataClear ( clock );
				}

				clock->data[clock->numdata++] = val;
			}

			if ( clock->numdata > 0 )
				loggerf ( LOGGER_TRACE, "pulse end: length "TIMEF_FORMAT" - %3d: %d\n", diff, clock->numdata-1, clock->data[clock->numdata-1] );

		}

		clock->status = status;
		clock->changetime = timef;
	}
	else if ( clock->status && !status )
	{
		loggerf ( LOGGER_TRACE, "pulse start: at "TIMEF_FORMAT"\n", timef );


		val = clkPulseLength ( diff );

		if ( val < 0 )
		{
			loggerf ( LOGGER_TRACE, "warning: bad clear length "TIMEF_FORMAT"\n", diff );

			clkDataClear ( clock );
		}
		else if ( (clock->numdata > 1) && (clock->data[clock->numdata-1] == 1) && (val == 1) )
		{
			//the MSF signal has a 2nd bit sometimes - flag it...
			clock->msf_skip_b = 1;
		}
		else if ( val == 18 || val == 19 )
		{
			clock->data[clock->numdata++] = 0;	//store the missing second 59 value

			clkDumpData ( clock );

			if ( dcf77Decode ( clock, timef ) < 0 )
				loggerf ( LOGGER_DEBUG, "Warning: failed to decode DCF77\n" );
			else
				clkSendTime ( clock );


			clkDataClear ( clock );

		}
		else
		{
			//we have the start of a second here...

//printf ( "\a" ); flush ( stdout );

			//TODO: PPS processing on this time
			//increment time by 1 second (if pulse is good - within 50 ms perhaps?)
			//and, keep a running average of the error from the current time
			clkProcessPPS ( clock, timef );

//			clkDumpPPS ( clock );
		}


		clock->status = status;
		clock->changetime = timef;
	}
	//else no change so ignore pulse (should never happen)
}

void
clkSendTime ( clkInfoT* clock )
{
	time_f	average, maxerr;

	if ( clkCalculatePPSAverage ( clock, &average, &maxerr ) < 0 )
	{
		maxerr = 0.005;

		loggerf ( LOGGER_DEBUG, "clock: radio time "TIMEF_FORMAT", pc time "TIMEF_FORMAT"\n", clock->radiotime, clock->pctime );

		if ( !debugLevel )
			shmStore ( clock->shm, clock->radiotime, clock->pctime, maxerr, clock->radioleap );
	}
	else
	{
		loggerf ( LOGGER_DEBUG, "clock: radio time "TIMEF_FORMAT", average pctime "TIMEF_FORMAT", error +-"TIMEF_FORMAT"\n", clock->radiotime, clock->radiotime + average, maxerr );

		if ( !debugLevel )
			shmStore ( clock->shm, clock->radiotime, clock->radiotime + average, maxerr, clock->radioleap );
	}

}

void
clkProcessPPS ( clkInfoT* clock, time_f timef )
{
//	time_f	average, maxerr;

	//cant process second pulses unless we have decoded the time...
	if ( clock->radiotime == 0 )
		return;

	clock->secondssincetime += 1.0;

	clock->ppslist[clock->ppsindex].pctime = timef;
	clock->ppslist[clock->ppsindex].radiotime = clock->radiotime + clock->secondssincetime;
	clock->ppsindex++;
	clock->ppsindex %= PPS_AVERAGE_COUNT;

//	if ( clkCalculatePPSAverage ( clock, &average, &maxerr ) >= 0 )
//	{
//
//	}
}




static int
sort_timef_compare ( const void* a, const void* b )
{
	time_f	ta,tb;

	ta = *(time_f*)a;
	tb = *(time_f*)b;

	if ( ta < tb )
		return -1;
	else if ( ta > tb )
		return +1;
	return 0;
}

int
clkCalculatePPSAverage ( clkInfoT* clock, time_f* paverage, time_f* pmaxerr )
{
	int	i;
	time_f	err;
	time_f	total_offset,average_offset;
	int	total_count;
	time_f	standard_deviation;
	time_f	timediff[PPS_AVERAGE_COUNT] = { 0.0 };


	for ( i=0; i<PPS_AVERAGE_COUNT; i++ )
	{
		if ( clock->ppslist[i].pctime == 0 || clock->ppslist[i].radiotime == 0 )
			return -1;

		err = clock->ppslist[i].pctime - clock->ppslist[i].radiotime;
		//if the time isn't close, don't bother tracking it...
		if ( fabs ( err ) > 0.1 )	//within 100ms - more than this and ntpd will step the time soon
			return -1;

		timediff[i] = err;
	}

	qsort ( timediff, PPS_AVERAGE_COUNT, sizeof(time_f), sort_timef_compare );


	total_offset = 0;
	total_count = 0;
	for ( i=PPS_AVERAGE_COUNT/4; i<PPS_AVERAGE_COUNT*3/4; i++ )
	{
		err = timediff[i];

		total_offset += err;
		total_count++;

	}

	average_offset = total_offset / total_count;

	standard_deviation = 0;
	for ( i=0; i<PPS_AVERAGE_COUNT; i++ )
	{
		if ( clock->ppslist[i].pctime == 0 || clock->ppslist[i].radiotime == 0 )
			continue;

		err = (clock->ppslist[i].pctime - clock->ppslist[i].radiotime) - average_offset;

		standard_deviation += err*err;
	}
	standard_deviation /= total_count;
	standard_deviation = sqrt ( standard_deviation );

	*paverage = average_offset;
	*pmaxerr = standard_deviation;	//over50%   * 2.0;	//over 90% of the values should be in this range...

	return 0;
}

