/*
 * Accelerator CPUS Interface
 *
 * Copyright 2020 SUSE LLC
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef GVM_CPUS_H
#define GVM_CPUS_H

#include "sysemu/cpus.h"

int gvm_init_vcpu(CPUState *cpu);
int gvm_cpu_exec(CPUState *cpu);
void gvm_destroy_vcpu(CPUState *cpu);
void gvm_cpu_synchronize_post_reset(CPUState *cpu);
void gvm_cpu_synchronize_post_init(CPUState *cpu);
void gvm_cpu_synchronize_pre_loadvm(CPUState *cpu);

#endif /* GVM_CPUS_H */
