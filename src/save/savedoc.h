/* TOML subset: unknown keys are written back (comments are not); strings hold U+0000..U+00FF as bytes. */
#ifndef NUWORLD_SAVE_SAVEDOC_H
#define NUWORLD_SAVE_SAVEDOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum NwDocType {
    NW_DOC_INT,
    NW_DOC_BOOL,
    NW_DOC_STR,
    NW_DOC_ARRAY,
} NwDocType;

typedef struct NwDocValue {
    NwDocType type;
    int hex;                  /* NW_DOC_INT: written as 0x with at least this many digits; 0 = decimal */
    int64_t i;                /* NW_DOC_INT, NW_DOC_BOOL */
    uint8_t *bytes;
    size_t len;               /* NW_DOC_STR: bytes; NW_DOC_ARRAY: items */
    struct NwDocValue *items;
} NwDocValue;

typedef struct NwDocKey {
    char *name;
    NwDocValue value;
} NwDocKey;

typedef struct NwDocTable {
    char **path; /* the header's keys; the root table has none */
    size_t depth;
    NwDocKey *keys;
    size_t count, cap;
} NwDocTable;

typedef struct NwDoc {
    NwDocTable **tables; /* tables[0] is the root table; table pointers stay valid until the table is dropped */
    size_t count, cap;
} NwDoc;

bool nw_doc_init(NwDoc *doc);
void nw_doc_free(NwDoc *doc);
/* Into a document just made by nw_doc_init; false with an SDL error "line N: ...". */
bool nw_doc_parse(NwDoc *doc, const char *text, size_t size);
/* comment: lines without the "# ", or NULL; SDL_free the result. */
char *nw_doc_write(const NwDoc *doc, const char *comment, size_t *size);

NwDocTable *nw_doc_table(const NwDoc *doc, const char *const *path, size_t depth);
/* Added when missing, after the last table that shares the longest prefix with it. */
NwDocTable *nw_doc_add_table(NwDoc *doc, const char *const *path, size_t depth);
void nw_doc_drop_table(NwDoc *doc, NwDocTable *t);

const NwDocValue *nw_doc_get(const NwDocTable *t, const char *name);
/* v is taken over (freed on failure); in place when name is there, else appended. */
bool nw_doc_set(NwDoc *doc, NwDocTable *t, const char *name, NwDocValue *v);
void nw_doc_unset(NwDocTable *t, const char *name);

NwDocValue nw_doc_int(int64_t i, int hex);
NwDocValue nw_doc_bool(bool b);
bool nw_doc_str(NwDocValue *v, const void *bytes, size_t len);
bool nw_doc_array(NwDocValue *v, size_t n);
void nw_doc_value_free(NwDocValue *v);

#endif
