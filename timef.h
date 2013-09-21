#ifndef TIMEF_H_
#define TIMEF_H_

#include <math.h>
#include "systime.h"

//a floating point time format for representing a time_t
typedef double time_f;

//printf() format for time_f values
#define	TIMEF_FORMAT	"%.6f"

//convert between 
#define	timeval2time_f(__timeval,__timef)	__timef = (time_f)(__timeval)->tv_sec + (time_f)(__timeval)->tv_usec / (time_f)1000000.0

#define	time_f2timeval(__timef,__timeval)	do { (__timeval)->tv_sec = floor((__timef)); (__timeval)->tv_usec = ((__timef) - (__timeval)->tv_sec)*1000000.0; } while(0)



#ifdef ENABLE_TIMEPPS

#define	timespec2time_f(__timeval,__timef)	__timef = (time_f)(__timeval)->tv_sec + (time_f)(__timeval)->tv_nsec / (time_f)1000000000.0

#define	time_f2timespec(__timef,__timeval)	do { (__timeval)->tv_sec = floor((__timef)); (__timeval)->tv_nsec = ((__timef) - (__timeval)->tv_sec)*1000000000.0; } while(0)

#endif

#endif
