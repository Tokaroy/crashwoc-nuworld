#ifndef NUWORLD_DATA_ELF_H
#define NUWORLD_DATA_ELF_H

#include <stddef.h>
#include <stdint.h>

/* Zeros outside the PT_LOAD file bytes (.sbss, .bss); NULL on success, else what is wrong with the file. */
const char *nw_elf_flatten(const void *file, size_t size, uint32_t base, uint32_t end, void *dst);

#endif
