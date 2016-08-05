#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(radioclkd2,0.07)
AM_INIT_AUTOMAKE(foreign)

AM_MAINTAINER_MODE

AC_CONFIG_SRCDIR([clock.c])

AC_CONFIG_HEADER([autoconf.h])
AC_CONFIG_FILES([Makefile])


# Checks for programs.
AC_PROG_CC
#AC_PROG_GCC_TRADITIONAL

# Checks for libraries.
#AC_CHECK_LIB([m], [log])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
#AC_HEADER_STDBOOL
AC_CHECK_HEADERS([sys/timepps.h sys/mman.h sched.h sys/ioctl.h fcntl.h syslog.h])

#AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_C_VOLATILE

AC_CHECK_DECLS([TIOCMIWAIT],,,[#include <sys/ioctl.h>])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MKTIME
AC_TYPE_SIGNAL

AC_CHECK_FUNCS([gettimeofday],,[AC_MSG_ERROR([We need gettimeofday - sub-second accuracy is essential])])
AC_CHECK_FUNCS([strcasecmp stricmp strcmpi],break,[AC_MSG_ERROR([We need strcasecmp, stricmp or strcmpi])])
AC_CHECK_FUNCS([tzset timegm setenv])
AC_CHECK_FUNCS([mlockall sched_get_priority_level sched_setscheduler])

#AC_FUNC_MALLOC
#AC_CHECK_FUNCS([memset setenv sqrt strchr])


AC_OUTPUT



if test "$ac_cv_func_timegm" = "no"
then
 cat <<END

Warning: timegm not available

There may be problems decoding the time correctly.
See README.timezone for details.

END

fi
