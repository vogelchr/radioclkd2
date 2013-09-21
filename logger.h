#ifndef LOG_H_
#define	LOG_H_

#include <stdio.h>

void loggerSetFile ( FILE* file, int level );
void loggerSyslog ( int flag, int level );

//log levels from 0 (always shown), and +1 for higher debug levels
#define	LOGGER_NOTE	(0)
#define	LOGGER_INFO	(1)
#define	LOGGER_DEBUG	(2)
#define	LOGGER_TRACE	(3)

void loggerf ( int level, char* format, ... );
//(note: loggerf(), rather than logger() to remind me that it's a printf() style function.
// too many other programs have that kind of mistake in them!)


#endif
