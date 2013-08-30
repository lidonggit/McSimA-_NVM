#ifndef INTERCEPT_MALLOC_H
#define INTERCEPT_MALLOC_H

#include "PTS.h"
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include "PthreadSim.h"

VOID InterceptMalloc_beg();
VOID InterceptMalloc_end();

VOID MallocBefore(CHAR *name, int size, ADDRINT returnip);

VOID MallocAfter(CHAR *name, ADDRINT ret_addr, ADDRINT returnip);

VOID FreeBefore(CHAR *name, ADDRINT addr);

VOID ReallocBefore(CHAR *name, ADDRINT ptr, int size, ADDRINT returnip);

VOID CallocBefore(CHAR *name, int nmemb, int size, ADDRINT returnip);

#ifdef APP_HPL
//VOID HPL_malloc_assist(ADDRINT addr, long size);
VOID HPL_malloc_assist(ADDRINT addr, int size);
#endif

#endif
