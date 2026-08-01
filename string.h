#ifndef STRING_H
#define STRING_H

#include <string.h>
#include "journal.h"
#include "myfs.h"

int count_l(const char *path);

char* parent_path(const char *path, int l);

char* get_name(const char *path);

char *split(const char *path, int n);

#endif
