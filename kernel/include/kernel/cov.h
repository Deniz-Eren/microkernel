/**
 * @file     cov.h
 * @brief    Kernel LLVM Raw Profile v8 collector and console framing interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

/* need:: ICD_DOCS_0002 */

struct pcb;

long sys_cov_write(uint64_t op, uint64_t arg);
void cov_dump_proc(struct pcb *p);
void cov_dump_kernel(void);
