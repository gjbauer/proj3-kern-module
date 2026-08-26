#ifndef TIME__H
#define TIME__H

#ifdef _KERNEL
#include <sys/types.h>
#else
#include <stdint.h>
#endif

uint64_t TIME(void);

#endif
