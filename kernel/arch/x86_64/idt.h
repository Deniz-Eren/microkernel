// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <stdint.h>
#include <kernel/trapframe.h>

void idt_init(void);
void interrupt_handler(trapframe_t *tf);
