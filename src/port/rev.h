#ifndef NUWORLD_PORT_REV_H
#define NUWORLD_PORT_REV_H

#include <stdbool.h>

extern int nw_rev;

bool nw_rev_set(const char *name);
const char *nw_rev_name(void);

#endif
