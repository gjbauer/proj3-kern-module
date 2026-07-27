#ifndef PRINT_H
#define PRINT_H

#ifdef _KERNEL
#define FPRINTF(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define FPRINTF(fmt, ...) fprintf(stderr, fmt __VA_OPT__(,) __VA_ARGS__)
#endif

#endif
