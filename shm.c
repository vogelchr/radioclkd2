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
#include <sys/shm.h>
#include <sys/ipc.h>
#include <math.h>
#include <stdio.h>

#include "shm.h"
#include "timef.h"
#include "logger.h"

shmTimeT*
shmCreate ( int unit )
{
	int	shmid;
	shmTimeT* shm;

	shmid = shmget ( SHM_KEY + unit, sizeof(shmTimeT), IPC_CREAT | 0700 );
	if ( shmid == -1 )
		return NULL;

	shm = (shmTimeT*) shmat ( shmid, 0, 0 );
	if ( (shm == (shmTimeT*)-1) || (shm == NULL) )
		return NULL;

	return shm;
}


void
shmStore ( shmTimeT* volatile shm, time_f radioclock, time_f localrecv, time_f time_err, int leap )
{
	struct timeval radioclocktv,localrecvtv;

	loggerf ( LOGGER_DEBUG, "shm: storing time "TIMEF_FORMAT" local "TIMEF_FORMAT" err "TIMEF_FORMAT" leap %d\n", radioclock, localrecv, time_err, leap );

	time_f2timeval ( radioclock, &radioclocktv );
	time_f2timeval ( localrecv, &localrecvtv );

	shm->valid = 0;

	shm->mode = 1;
	shm->count++;
	shm->clockTimeStampSec = radioclocktv.tv_sec;
	shm->clockTimeStampUSec = radioclocktv.tv_usec;
	shm->receiveTimeStampSec = localrecvtv.tv_sec;
	shm->receiveTimeStampUSec = localrecvtv.tv_usec;
	shm->leap = leap;
	shm->precision = log(time_err)/log(2);
	shm->count++;

	shm->valid = 1;
}

void
shmCheckNoStore ( shmTimeT* volatile shm )
{
	if ( shm->valid )
		return;

	shm->mode = 1;
	shm->count++;
	shm->clockTimeStampSec = 0;
	shm->clockTimeStampUSec = 0;
	shm->receiveTimeStampSec = 0;
	shm->receiveTimeStampUSec = 0;
	shm->leap = LEAP_NOTINSYNC;
	shm->precision = 0;
	shm->count++;

	shm->valid = 1;
	
}
