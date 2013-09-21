#ifndef SHM_H_
#define SHM_H_

#include "timef.h"

// ntpd shared memory reference clock driver structure
#define SHM_KEY 0x4e545030
typedef struct {
	int     mode;
	int     count;
	time_t  clockTimeStampSec;
	int     clockTimeStampUSec;
	time_t  receiveTimeStampSec;
	int     receiveTimeStampUSec;
	int     leap;
	int     precision;
	int     nsamples;
	int     valid;
	int     dummy[10];
} shmTimeT;


#define	LEAP_NOWARNING	0x0	/* normal, no leap second warning */
#define	LEAP_ADDSECOND	0x1	/* last minute of day has 61 seconds (also used when leap-second direction is unknown) */
#define	LEAP_DELSECOND	0x2	/* last minute of day has 59 seconds */
#define	LEAP_NOTINSYNC	0x3	/* overload, clock is free running */



shmTimeT* shmCreate ( int unit );
void shmStore ( shmTimeT* volatile shm, time_f radioclock, time_f localrecv, time_f time_err, int leap );
void shmCheckNoStore ( shmTimeT* volatile shm );


#endif
