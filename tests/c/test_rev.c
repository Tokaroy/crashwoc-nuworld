#include <stdio.h>
#include <string.h>

#include "nu_port.h"
#include "port/rev.h"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    CHECK(nw_rev == 0 && NW_REV(1, 2) == 1 && strcmp(nw_rev_name(), "1.03") == 0);
    CHECK(nw_rev_set("late") && nw_rev == 1 && NW_REV(1, 2) == 2 && strcmp(nw_rev_name(), "late") == 0);
    CHECK(!nw_rev_set("2.01") && nw_rev == 1);
    CHECK(nw_rev_set("1.03") && nw_rev == 0 && NW_REV(1, 2) == 1);

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
