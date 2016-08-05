#ifndef CLOCK_H_
#define CLOCK_H_

#include "systime.h"
#include "timef.h"
#include "shm.h"


#define	PPS_AVERAGE_COUNT		(60)

#define CLOCKTYPE_DCF77	0
#define CLOCKTYPE_MSF	1
#define CLOCKTYPE_WWVB	2


typedef struct clkInfoS clkInfoT;
struct clkInfoS
{
	clkInfoT*	next;

	int	inverted;	//if true, treat the signal as inverted...
	time_f	fudgeoffset;	//added to the recieved time - used to correct for recieve delays

	int	status;
	time_f	changetime;


	//store 2 minutes of data - there will be a complete minute of data in here somewhere...
	signed char	data[120];
	int		numdata;

	int		msf_skip_b;	//set to 1 if we have a 100ms high after a 100ms low

	time_f		pctime;
	time_f		radiotime;
	int		radioleap;
	int		clocktype;	// The clock type. See CLOCKTYPE_

	int	secondssincetime;

	struct
	{
		time_f	pctime;
		time_f	radiotime;
	} ppslist[PPS_AVERAGE_COUNT];
	int	ppsindex;

	shmTimeT*	shm;
};


void clkDumpData ( const clkInfoT* clock );

clkInfoT* clkCreate ( int inverted, int shmunit, time_f fudgeoffset, int clocktype );

void clkDataClear ( clkInfoT* clock );

int clkPulseLength ( time_f timef, int clocktype );


void clkProcessStatusChange ( clkInfoT* clock, int Status, time_f timef );

void clkSendTime ( clkInfoT* clock );

void clkProcessPPS ( clkInfoT* clock, time_f timef );

//void clkDumpPPS ( clkInfoT* clock );

int clkCalculatePPSAverage ( clkInfoT* clock, time_f* paverage, time_f* pdeviation );


#endif
