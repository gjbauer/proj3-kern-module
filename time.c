#include "time.h"

#ifdef _KERNEL
#include <sys/time.h>
#else
#include <time.h>
#endif

uint64_t TIME(void)
{
#ifdef _KERNEL
	struct timespec tsp;
	getnanotime(&tsp);
	return tsp.tv_nsec;
#else
	return time(NULL);
#endif
}
