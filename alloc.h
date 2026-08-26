#ifndef ALLOC_H
#define ALLOC_H

#ifdef _KERNEL
#include <sys/libkern.h>
#else
#include <string.h>
#endif

char *STRDUP(const char *str);

void FREE(void *addr);

void *MALLOC(size_t size);

#endif
