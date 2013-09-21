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

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "systime.h"
#include <sys/ioctl.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/errno.h>

#ifdef ENABLE_TIMEPPS
#include <sys/timepps.h>
#endif



#include "logger.h"

#include "serial.h"
#include "memory.h"


static serDevT* 	serDevHead;
static serLineT*	serLineHead;


int
serInit (void)
{
	serDevHead = NULL;
	serLineHead = NULL;

	return 0;
}

serLineT*
serAddLine ( char* dev, int line, int mode )
{
	char		fulldev[64];
	serDevT*	serdev;
	serLineT*	serline;

	//allow for either full paths or /dev relative paths...
	if ( dev[0] == '/' )
	{
		if ( strlen(dev) >= 64 )
		{
			loggerf ( LOGGER_NOTE, "serAddLine(): dev too long\n" );
			return NULL;
		}
		strcpy ( fulldev, dev );
	}
	else
	{
		strcpy ( fulldev, "/dev/" );
		if ( strlen(dev)+strlen(fulldev) >= 64 )
		{
			loggerf ( LOGGER_NOTE, "serAddLine(): dev too long\n" );
			return NULL;
		}
		strcat ( fulldev, dev );
	}

	//make sure only one line bit is set...
	if ( line & (line-1) )
	{
		loggerf ( LOGGER_NOTE, "serAddLine(): more than one line bit set\n" );
		return NULL;
	}

	//try and find an existing device in the list...
	serdev = serDevHead;
	while ( serdev != NULL )
	{
		if ( strcmp ( serdev->dev, fulldev ) == 0 )
			break;
		serdev = serdev->next;
	}
	if ( serdev == NULL )
	{
		//no existing device - create a new one..
		serdev = safe_mallocz ( sizeof(serDevT) );
		serdev->next = serDevHead;
		serDevHead = serdev;

		strcpy ( serdev->dev, fulldev );
		serdev->mode = mode;
		serdev->modemlines = 0;
		serdev->fd = -1;
	}

	//make sure we're using it in the same mode...
	if ( serdev->mode != mode )
	{
		loggerf ( LOGGER_NOTE, "serAddLine(): cannot add line for same device with different mode\n" );
		return NULL;
	}


	//make sure we're not already monitoring this line...
	if ( serdev->modemlines & line )
	{
		loggerf ( LOGGER_NOTE, "serAddLine(): cannot add modem status line more than once\n" );
		return NULL;
	}

	//ok - we've got a valid device/line/mode combo...

	serdev->modemlines |= line;

	//create a new line entry...
	serline = safe_mallocz ( sizeof(serLineT) );
	serline->next = serLineHead;
	serLineHead = serline;

	serline->dev = serdev;
	serline->line = line;

	return serline;

}



serDevT*
serGetDev ( serDevT* prev )
{
	if ( prev == NULL )
		return serDevHead;

	return prev->next;
}

serLineT*
serGetLine ( serLineT* prev )
{
	if ( prev == NULL )
		return serLineHead;

	return prev->next;
}


int
serOpenDev ( serDevT* dev )
{
#ifdef ENABLE_TIMEPPS
	pps_params_t	ppsparams;
	int		ppsmode;
#endif

	dev->fd = open ( dev->dev, O_RDONLY|O_NOCTTY );

	switch ( dev->mode )
	{
#ifdef ENABLE_TIMEPPS
	case SERPORT_MODE_TIMEPPS:
		if ( time_pps_create ( dev->fd, &dev->ppshandle ) == -1 )
			return -1;

		if ( time_pps_getparams ( dev->ppshandle, &ppsparams ) == -1 )
			return -1;

		ppsparams.mode |= PPS_TSFMT_TSPEC | PPS_CAPTUREBOTH;

		if ( time_pps_setparams ( dev->ppshandle, &ppsparams ) == -1 )
			return -1;

		if ( time_pps_getcap ( dev->ppshandle, &ppsmode ) == -1 )
			return -1;

		//NOTE: these should probably be error cases, but the code is still experimental and
		//the PPS support I've used also seems to have problems
		if ( ! (ppsmode | PPS_CAPTUREASSERT) )
			loggerf ( LOGGER_NOTE, "Warning: PPS_CAPTUREASSERT not supported\n" );
		if ( ! (ppsmode | PPS_CAPTURECLEAR) )
			loggerf ( LOGGER_NOTE, "Warning: PPS_CAPTURECLEAR not supported\n" );

//!!! no point using this - too many problems
//1. linux doesn't report PPS_CANWAIT, but can
//2. FreeBSD does report PPS_CANWAIT, but can't
//(unless its me reading the docs backwards?)
//		if ( ! (ppsmode | PPS_CANWAIT) )
//			loggerf ( LOGGER_NOTE, "Warning: PPS_CANWAIT not supported (linux lies)\n" );

#endif
	}

	return dev->fd;
}

int
serInitHardware ( serDevT* dev )
{
	if ( dev->fd < 0 )
		serOpenDev ( dev );

	if ( dev->fd < 0 )
		return -1;

	//add code to power on the device (set DTR high - maybe other options..?)

	//wait for the device to power up...
	sleep(3);

	return 0;
}


//static jmp_buf alrmjump;

#ifdef ENABLE_TIOCMIWAIT
static void
sigalrm ( int sig )
{
	//empty func - just used so that ioctl() aborts on an alarm()
}
#endif

int
serWaitForSerialChange ( serDevT* dev )
{
	struct timeval tv;
	time_f	timef;
	int	i,ret;
#ifdef ENABLE_TIMEPPS
	struct timespec timeout;
	pps_info_t	ppsinfo;
	int		ppslines;
#endif

	if ( dev->modemlines == 0 )
		return -1;

	switch ( dev->mode )
	{
	case SERPORT_MODE_POLL:

		for ( i=0; i<10*1000; i++ )
		{
			gettimeofday ( &tv, NULL );
			timeval2time_f ( &tv, timef );
			ret = serGetDevStatusLines ( dev, timef );
			if ( ret < 0 )
				return -1;
			if ( ret == 1 )
				return 0;

			usleep ( 1000 );
		}

		//timeout
		return -1;
		break;


#ifdef ENABLE_TIOCMIWAIT
	case SERPORT_MODE_IWAIT:
		signal ( SIGALRM, sigalrm );
		alarm ( 10 );

		if ( ioctl ( dev->fd, TIOCMIWAIT, dev->modemlines) != 0 )
			return -1;
		gettimeofday ( &tv, NULL );
		timeval2time_f ( &tv, timef );

		signal ( SIGALRM, SIG_DFL );
		alarm ( 0 );

		if ( serGetDevStatusLines ( dev, timef ) < 0 )
			return -1;

		return 0;
		break;
#endif

#ifdef ENABLE_TIMEPPS
	case SERPORT_MODE_TIMEPPS:
		timeout.tv_sec = 0;
		timeout.tv_nsec = 0;

		for ( i=0; i<10*100; i++ )
		{
			if ( time_pps_fetch ( dev->ppshandle, PPS_TSFMT_TSPEC, &ppsinfo, &timeout ) == -1 )
			{
				loggerf ( LOGGER_NOTE, "ppsfetch failed: %d\n", errno );
				return -1;
			}

			if ( ppsinfo.assert_sequence != dev->ppslastassert )
			{
				timespec2time_f ( &ppsinfo.assert_timestamp, timef );
				ppslines = TIOCM_CD;	//NOTE: assuming that pps support is on the DCD line
				dev->ppslastassert = ppsinfo.assert_sequence;

				if ( serStoreDevStatusLines ( dev, ppslines, timef ) < 0 )
					return -1;
				return 0;
			}
			else if ( ppsinfo.clear_sequence != dev->ppslastclear )
			{
				timespec2time_f ( &ppsinfo.clear_timestamp, timef );
				ppslines = 0;
				dev->ppslastclear = ppsinfo.clear_sequence;

				if ( serStoreDevStatusLines ( dev, ppslines, timef ) < 0 )
					return -1;
				return 0;
			}

			usleep ( 10000 );
		}

		return -1;
		break;
#endif

	}

	loggerf ( LOGGER_NOTE, "Error: serWaitForSerialChange(): mode not supported\n" );

	//unknown serial port mode !!
	return -1;

}

int
serGetDevStatusLines ( serDevT* dev, time_f timef )
{
	int	lines;

	if ( ioctl ( dev->fd, TIOCMGET, &lines ) != 0 )
		return -1;

	return serStoreDevStatusLines ( dev, lines, timef );
}

int
serStoreDevStatusLines ( serDevT* dev, int lines, time_f timef )
{
	if ( lines != dev->curlines )
	{
		dev->prevlines = dev->curlines;
		dev->curlines = lines;
		dev->eventtime = timef;

		return 1;
	}

	return 0;
}


int
serUpdateLinesForDevice ( serDevT* dev )
{
	serLineT* line;

	for ( line = serGetLine ( NULL );  line != NULL; line = serGetLine ( line ) )
	{
		//skip lines not on this device...
		if ( line->dev != dev )
			continue;

		if ( (dev->curlines & line->line) != (dev->prevlines & line->line) )
		{
			line->curstate = dev->curlines & line->line;
			line->eventtime = dev->eventtime;
		}
	}

	return 0;
}
