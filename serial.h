#ifndef SERIAL_H_
#define SERIAL_H_

#include "systime.h"
#include <sys/ioctl.h>

#include "timef.h"

#ifdef ENABLE_TIMEPPS
#include <sys/timepps.h>
#endif


//a serial device (serDevT) is an individual serial port, with several status control lines
//a modem status line (serLineT) is an individual status line on a serial port

typedef struct serDevS serDevT;
typedef struct serLineS serLineT;

struct serDevS
{
	serDevT*	next;

	//full device name (including "/dev/")
	char		dev[64];

#define	SERPORT_MODE_IWAIT	(1)
#define	SERPORT_MODE_POLL	(2)
#define	SERPORT_MODE_TIMEPPS	(3)
	int		mode;

	//which modem status lines to check - some of TIOCM_{RNG|DSR|CD|CTS}
	int		modemlines;

	//-- runtime data

	//once opened, the fd for this device
	int		fd;
#ifdef ENABLE_TIMEPPS
	pps_handle_t	ppshandle;
	int		ppslastassert;
	int		ppslastclear;
#endif

	//the current and previous modem lines active - some of modemlines
	int		curlines;
	int		prevlines;
	time_f		eventtime;

};

struct serLineS
{
	serLineT*	next;

	//one of TIOCM_{RNG|DSR|CD|CTS}
	int		line;
	serDevT*	dev;

	int		curstate;
	time_f		eventtime;

};

int serInit (void);
serLineT* serAddLine ( char* dev, int line, int mode );

//pass in NULL to get the first dev/line
//pass in dev/line to get next dev/line
//returns NULL at end of dev/line list
serDevT* serGetDev ( serDevT* prev );
serLineT* serGetLine ( serLineT* prev );


int serInitHardware ( serDevT* dev );
int serWaitForSerialChange ( serDevT* dev );

int serGetDevStatusLines ( serDevT* dev, time_f timef );
int serStoreDevStatusLines ( serDevT* dev, int lines, time_f time );

int serUpdateLinesForDevice ( serDevT* dev );


#endif
