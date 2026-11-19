/* First in every engine and game unit's wrapper (tools/tu_filter.py), so units skipping nu_libc.h see prototypes. */
#ifndef NUWORLD_PORT_NW_PRELUDE_H
#define NUWORLD_PORT_NW_PRELUDE_H

#include "nu_libc.h"

#include "port/nw_ee.h"
#include "port/nw_newlib.h"

/* UCRT has its own strlwr, x86_64 libgcc no __divdi3 (src/shim/libc.c); after nu_libc.h so host headers keep theirs. */
#define strlwr nw_strlwr
#define __divdi3 nw_divdi3

int nw_decomp_printf(const char *fmt, ...);
#define printf nw_decomp_printf

#endif
