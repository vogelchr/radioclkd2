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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#ifdef ENABLE_SCHED
#include <sched.h>
#endif

#ifdef ENABLE_MLOCKALL
#include <sys/mman.h>
#endif

#include "settings.h"
#include "logger.h"
#include "clock.h"
#include "serial.h"
#include "memory.h"


#if !HAVE_STRCASECMP
# if HAVE_STRICMP
#  define strcasecmp(a,b) stricmp((a),(b))
# else
#  define strcasecmp(a,b) strcmpi((a),(b))
# endif
#endif

typedef struct
{
	char*		name;
	serLineT*	serline;
	clkInfoT*	clock;
} serClockT;

#define	MAX_CLOCKS		(16)
serClockT	clocklist[MAX_CLOCKS];



void StartClocks ( serDevT* serdev );



void
setRealtime (void)
{
#ifdef ENABLE_SCHED
	struct sched_param schedp;
#endif


#ifdef ENABLE_SCHED
	/* set realtime scheduling priority */
	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = sched_get_priority_max(SCHED_FIFO);	
	if (sched_setscheduler(0, SCHED_FIFO, &schedp)!=0)
		loggerf ( LOGGER_INFO, "error unable to set real time scheduling");
#else
	nice ( -20 );
#endif

#ifdef ENABLE_MLOCKALL
	/* lock all memory pages */
	if (mlockall(MCL_CURRENT | MCL_FUTURE) !=0)
		loggerf ( LOGGER_INFO, "error unable to lock memory pages");
#endif

}


void
setDemon (void)
{
	int	pid;

	pid = fork();
	if ( pid < 0 )
	{
		loggerf ( LOGGER_NOTE, "fork() failed\n" );
		exit(1);
	}
	if ( pid > 0 )
	{
		//parent
		exit(0);
	}
	else
	{
		//child
		loggerf ( LOGGER_INFO, "entering demon mode\n" );
		if ( setsid() < 0 )
			loggerf ( LOGGER_NOTE, "setsid() failed\n" );

	}
}

void
usage (void)
{
	printf (
"Usage: radioclkd [ -s poll|iwait|timepps ] [ -d ] [ -v ] tty[:[-]line[:fudgeoffs]] ...\n"
"   -s poll: poll the serial port 1000 times/sec (poor)\n"
"   -s iwait: wait for serial port interrupts (ok)\n"
"   -s timepps: use the timepps interface (good)\n"
#ifndef ENABLE_TIMEPPS
"  (timepps not available)\n"
#endif
#ifndef ENABLE_TIOCMIWAIT
"  (iwait not available)\n"
#endif
"   -d: debug mode. runs in the foreground and print pulses\n"
"   -v: verbose mode.\n"
"   tty: serial port for clock\n"
"   line: one of dcd, cts, dsr or rng - default is dcd\n"
"   (if - specified, treat signal as inverted\n"
"   fudgeoffs: fudge time, in seconds\n"
		);

	exit(1);
}

int
main ( int argc, char** argv )
{
	int	serialmode;
	int	shmunit;
	char*	arg;
	char*	parm;
	serDevT*	devfirst;
	serDevT*	devnext;


	loggerSetFile ( stderr, LOGGER_DEBUG );
	loggerSyslog ( 1, LOGGER_INFO );

	loggerf ( LOGGER_INFO, "version %s\n", VERSION );


#if defined(ENABLE_TIMEPPS)
	serialmode = SERPORT_MODE_TIMEPPS;
#elif defined(ENABLE_TIOCMIWAIT)
	serialmode = SERPORT_MODE_IWAIT;
#else
	serialmode = SERPORT_MODE_POLL;
#endif

	shmunit = 0;


	if ( argc < 2 )
		usage();

	//skip the program name
	argc--;
	argv++;

	while ( argc > 0 )
	{
		arg = argv[0];

		if ( arg[0] == '-' )
		{
			switch ( arg[1] )
			{
			case 's':
				if ( strlen(arg) > 2 )
				{
					parm = arg + 2;
				}
				else
				{
					argc--;
					argv++;
					parm = argv[0];
				}

				if ( strcasecmp ( parm, "poll" ) == 0 )
					serialmode = SERPORT_MODE_POLL;
#ifdef ENABLE_TIMEPPS
				else if ( strcasecmp ( parm, "timepps" ) == 0 )
					serialmode = SERPORT_MODE_TIMEPPS;
#endif
#ifdef ENABLE_TIOCMIWAIT
				else if ( strcasecmp ( parm, "iwait" ) == 0 )
					serialmode = SERPORT_MODE_IWAIT;
#endif
				else
					usage();
				break;

			case 'd':
				debugLevel ++;
				break;

			case 'v':
				verboseLevel ++;
				break;

			default:
				usage();
				break;
			}
		}
		else
		{
			//arg = "tty[:[-]line]"
			char*	dev;
			int	line;
			int	negate;
			char*	linestr;
			char*	fudgestr;
			serLineT*	serline;
			clkInfoT*	clock;
			time_f	fudgeoffset;

			negate = 0;
			fudgeoffset = 0.0;
			line = TIOCM_CD;


			dev = safe_xstrcpy ( arg, -1 );
			linestr = strchr ( dev, ':' );
			if ( linestr != NULL )
			{

				//tty:
				*linestr = 0;	//terminate dev at the colon
				linestr++;		//and move past it...

				fudgestr = strchr ( linestr, ':' );

				if ( fudgestr != NULL )
				{
					*fudgestr = 0;
					fudgestr++;

					fudgeoffset = atof ( fudgestr );
				}

				if ( *linestr == '-' )
				{
					negate = !negate;
					linestr++;
				}

				if ( strcasecmp ( linestr, "cd" ) == 0 || strcasecmp ( linestr, "dcd" ) == 0 )
					line = TIOCM_CD;
				else if ( strcasecmp ( linestr, "cts" ) == 0 )
					line = TIOCM_CTS;
				else if ( strcasecmp ( linestr, "dsr" ) == 0 )
					line = TIOCM_DSR;
				else if ( strcasecmp ( linestr, "rng" ) == 0 )
					line = TIOCM_RNG;
				else
				{
					line = TIOCM_CD;
					loggerf ( LOGGER_NOTE, "Error: unknown serial port line '%s' - using DCD line instead\n", linestr );
				}

			}
	

			//right - we've got the serial port details - store them...

			serline = serAddLine ( dev, line, serialmode );
			if ( serline == NULL )
				loggerf ( LOGGER_NOTE, "Error: failed to attach to serial line '%s'\n", arg );

			clock = clkCreate ( negate, shmunit, fudgeoffset );
			if ( clock == NULL )
				loggerf ( LOGGER_NOTE, "Error: failed to create clock for serial line '%s'\n", arg );


			if ( clock != NULL && serline != NULL )
			{
				clocklist[shmunit].name = safe_xstrcpy ( arg, -1 );
				clocklist[shmunit].serline = serline;
				clocklist[shmunit].clock = clock;

				loggerf ( LOGGER_INFO, "Added clock unit %d on line '%s'\n", shmunit, arg );

				shmunit++;
			}
		}

		argc--;
		argv++;
	}



	if ( !debugLevel )
	{
		//non-debug mode - close stderr logging, fork, and set realtime priority

		loggerSetFile ( NULL, 0 );
		switch ( verboseLevel )
		{
		case 0:
			loggerSyslog ( 1, LOGGER_INFO );
			break;
		case 1:
		default:
			loggerSyslog ( 1, LOGGER_DEBUG );
			break;
		}
		setDemon();
		setRealtime();
	}
	else
	{
		switch ( verboseLevel )
		{
		case 0:
			loggerSetFile ( stderr, LOGGER_DEBUG );
			break;
		case 1:
		default:
			loggerSetFile ( stderr, LOGGER_TRACE );
			break;
		}
		loggerSyslog ( 0, 0 );
	}

//right - we're ready to start...
//now, due to the fact that we can only watch one serial port at a time, we'll have
//to fork off for all but the first serial port

	devfirst = serGetDev ( NULL );

	devnext = devfirst;

	//we don't want to do this if we're debugging clocks...
	if ( !debugLevel )
	{

		while ( (devnext = serGetDev(devnext)) != NULL )
		{
			int	pid;

			pid = fork();

			if ( pid < 0 )
			{
				loggerf ( LOGGER_INFO, "fork() failed for other devices\n" );
				//as we have forked above, this will kill all processes in the current process group (ie, just us)
				kill ( 0, SIGTERM );
			}
			else if ( pid == 0 )
			{
				//child
				StartClocks ( devnext );

				loggerf ( LOGGER_INFO, "child process terminated\n" );
				exit(0);
			}
			//else parent - continue on
		}
	}
	else
	{
		if ( serGetDev(devnext) != NULL )
			loggerf ( LOGGER_INFO, "Additional serial lines ignored in debug mode\n" );
	}

	StartClocks ( devfirst );

	loggerf ( LOGGER_INFO, "parent process terminated\n" );
	exit(1);


	return 0;	//to stop warnings
}


void
StartClocks ( serDevT* serdev )
{
	serLineT*	serline;
	int		c;


	loggerf ( LOGGER_INFO, "pid %d for device %s\n", getpid(), serdev->dev );

	if ( serInitHardware ( serdev ) < 0 )
	{
		loggerf ( LOGGER_INFO, "error initialising serial device\n" );
		return;
	}

	while(1)
	{
		if ( serWaitForSerialChange ( serdev ) < 0 )
		{
			loggerf ( LOGGER_DEBUG, "no serial line change\n" );
			continue;
		}

		serUpdateLinesForDevice ( serdev );

		serline = NULL;
		while ( (serline = serGetLine(serline)) != NULL )
		{
			for ( c = 0; c<MAX_CLOCKS; c++ )
			{
				if ( (serline->dev == serdev)
				  && (clocklist[c].serline == serline) )
				{
					clkProcessStatusChange ( clocklist[c].clock, serline->curstate, serline->eventtime );

				}
			}
		}

	}

}
