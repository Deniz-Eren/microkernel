#pragma once
#include <stddef.h>

void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
void *malloc(size_t n);
void free(void *p);
void *calloc(size_t a, size_t b);
long strtol(const char *s, char **end, int base);
int atoi(const char *s);
