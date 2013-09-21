#ifndef CONFIG_H_
#define CONFIG_H_

#include "autoconf.h"



#if HAVE_DECL_TIOCMIWAIT && HAVE_ALARM
// ioctl(serialfd,TIOCMIWAIT,..) waits for a serial interrupt
// alarm() is used as a timeout on the above
# define ENABLE_TIOCMIWAIT
#endif


#if HAVE_SYS_TIMEPPS_H
// if <sys/timepps.h> is available, enable pps code
# define ENABLE_TIMEPPS
#endif


#if HAVE_MLOCKALL && HAVE_SYS_MMAN_H
// 
# define ENABLE_MLOCKALL
#endif

#if HAVE_SCHED_H && HAVE_SCHED_GET_PRIORITY_LEVEL && HAVE_SCHED_SETSCHEDULER
# define ENABLE_SCHED
#endif

#define ENABLE_GPIO

#endif
