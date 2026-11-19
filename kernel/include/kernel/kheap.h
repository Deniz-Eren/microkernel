#pragma once
#include <stddef.h>

void kheap_init(void);
void *kmalloc(size_t n);
void *kzalloc(size_t n);
void kfree(void *p);
