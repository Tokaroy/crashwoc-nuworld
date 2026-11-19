#include <string.h>

#include "nu_types.h"
#include "game/credits.retail.h"

/* Declared by no header */
struct credit_s {
    char *txt;
    short colour;
    short size;
};
extern struct credit_s CreditLate[];

void InitCredits(void)
{
    for (struct credit_s *credit = CreditLate; credit->size > 0; credit++) {
        if (credit->txt && strcmp(credit->txt, "N-TROPHY") == 0) {
            credit->txt = "N-TROPY";
        } else if (credit->txt && strcmp(credit->txt, "N. TROPHY AND KAZENOMAJIN") == 0) {
            credit->txt = "N. TROPY AND KAZENOMAJIN";
        }
    }
    InitCredits__retail();
}
