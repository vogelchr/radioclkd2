#ifndef MEMORY_H_
#define MEMORY_H_

#include <sys/types.h>


void* safe_mallocz ( size_t size );
void safe_free ( void* ptr );

//malloc() memory for a new string and copies str into it.
//if len >=0, only copy that many characters, len<0 the whole string
//will always nul-terminate the result
char* safe_xstrcpy ( char* str, int len );

#endif
