/* Stubs (tools/gen_stubs.py) and near-match probes (tools/tu_filter.py); any thread may hit one. */
#ifndef NUWORLD_PORT_STUB_H
#define NUWORLD_PORT_STUB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct NwStubSite {
    const char *unit;
    const char *name;
    const char *why;
    uint32_t address;
    bool probe;
    unsigned hits;
    unsigned order;
    struct NwStubSite *next;
} NwStubSite;

/* Logs the first hit; then aborts unless NUWORLD_STUBS=continue or nw_stub_set_continue(true). */
void nw_stub_hit(NwStubSite *site);
/* Counts the call; never logs or aborts. */
void nw_near_hit(NwStubSite *site);
void nw_stub_set_continue(bool on);
/* One line per site in first-hit order, read by tools/stub_hits.py; returns stubs, *probes the near-match probes. */
unsigned nw_stub_report(FILE *out, unsigned *probes);

#define NW_STUB(unit, name, address, why) \
    do { \
        static NwStubSite nw_stub_site_ = { unit, name, why, address, false, 0, 0, 0 }; \
        nw_stub_hit(&nw_stub_site_); \
    } while (0)

#define NW_NEAR(unit, name, address) \
    do { \
        static NwStubSite nw_near_site_ = { unit, name, NULL, address, true, 0, 0, 0 }; \
        nw_near_hit(&nw_near_site_); \
    } while (0)

#endif
