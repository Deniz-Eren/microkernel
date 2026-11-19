#pragma once
#include <stddef.h>
#include <stdint.h>

void pmm_init(void);
uint64_t pmm_alloc_frame(void);
int pmm_alloc_contiguous(size_t count, uint64_t *out_base);
void pmm_free_frame(uint64_t phys);
void pmm_reserve(uint64_t base, uint64_t len);
uint64_t pmm_free_bytes(void);
uint64_t pmm_total_bytes(void);
