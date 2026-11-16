#include <stdio.h>
#include <string.h>

#include "nu_port.h"
#include "port/rev.h"

static int failures;
static unsigned char ram[16];

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static void *host(uint32_t addr)
{
    return addr >= 0x1000 && addr < 0x1000 + sizeof(ram) ? &ram[addr - 0x1000] : NULL;
}

int main(void)
{
    CHECK(nw_rev == 1 && NW_REV(1, 2) == 2 && strcmp(nw_rev_name(), "late") == 0);
    CHECK(nw_rev_set("late") && nw_rev == 1 && NW_REV(1, 2) == 2 && strcmp(nw_rev_name(), "late") == 0);
    CHECK(!nw_rev_set("2.01") && nw_rev == 1);
    CHECK(nw_rev_set("1.03") && nw_rev == 0 && NW_REV(1, 2) == 1);

    static const NwRevValue values[] = {
        { "a", 0x1000u, 4, 0x11223344u, 0x55667788u, 0x55667788u },
        { "b", 0x1004u, 1, 0u, 10u, 12u },
        { "c", 0x1008u, 2, 0x1234u, 1u, 2u },
        { "d", 0x2000u, 1, 0u, 1u, 1u },
    };
    memset(ram, 0, sizeof(ram));
    ram[0] = 0x44, ram[1] = 0x33, ram[2] = 0x22, ram[3] = 0x11, ram[8] = 0x99;
    CHECK(nw_rev_apply(values, 4, 60, host) == 2);
    CHECK(ram[0] == 0x88 && ram[3] == 0x55 && ram[4] == 12 && ram[8] == 0x99);
    memset(ram, 0, sizeof(ram));
    CHECK(nw_rev_apply(values + 1, 1, 50, host) == 0 && ram[4] == 10);

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
