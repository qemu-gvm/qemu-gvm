/*
 * QEMU GVM support
 *
 * Copyright IBM, Corp. 2008
 *           Red Hat, Inc. 2008
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *  Glauber Costa     <gcosta@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "sysemu/gvm_int.h"
#include "sysemu/runstate.h"
#include "sysemu/cpus.h"
#include "qemu/guest-random.h"
#include "qapi/error.h"

#include "gvm-accel-ops.h"

static void *gvm_vcpu_thread_fn(void *arg)
{
    CPUState *cpu = arg;
    int r;

    rcu_register_thread();

    qemu_mutex_lock_iothread();
    qemu_thread_get_self(cpu->thread);
    cpu->thread_id = qemu_get_thread_id();
    cpu->can_do_io = 1;
    current_cpu = cpu;

    r = gvm_init_vcpu(cpu);
    if (r < 0) {
        fprintf(stderr, "gvm_init_vcpu failed: %s\n", strerror(-r));
        exit(1);
    }

    /* signal CPU creation */
    cpu_thread_signal_created(cpu);
    qemu_guest_random_seed_thread_part2(cpu->random_seed);

    do {
        if (cpu_can_run(cpu)) {
            r = gvm_cpu_exec(cpu);
            if (r == EXCP_DEBUG) {
                cpu_handle_guest_debug(cpu);
            }
        }
        qemu_wait_io_event(cpu);
    } while (!cpu->unplug || cpu_can_run(cpu));

    gvm_destroy_vcpu(cpu);
    cpu_thread_signal_destroyed(cpu);
    qemu_mutex_unlock_iothread();
    rcu_unregister_thread();
    return NULL;
}

static void gvm_start_vcpu_thread(CPUState *cpu)
{
    char thread_name[VCPU_THREAD_NAME_SIZE];

    cpu->thread = g_malloc0(sizeof(QemuThread));
    cpu->halt_cond = g_malloc0(sizeof(QemuCond));
    qemu_cond_init(cpu->halt_cond);
    snprintf(thread_name, VCPU_THREAD_NAME_SIZE, "CPU %d/GVM",
             cpu->cpu_index);
    qemu_thread_create(cpu->thread, thread_name, gvm_vcpu_thread_fn,
                       cpu, QEMU_THREAD_JOINABLE);
#ifdef _WIN32
    cpu->hThread = qemu_thread_get_handle(cpu->thread);
#endif
}

static bool gvm_vcpu_thread_is_idle(CPUState *cpu)
{
    return false;
}

static void gvm_kick_vcpu_thread(CPUState *cpu)
{
    cpu_exit(cpu);
    gvm_raise_event(cpu);
}

static void gvm_accel_ops_class_init(ObjectClass *oc, void *data)
{
    AccelOpsClass *ops = ACCEL_OPS_CLASS(oc);

    ops->create_vcpu_thread = gvm_start_vcpu_thread;
    ops->cpu_thread_is_idle = gvm_vcpu_thread_is_idle;
    ops->synchronize_post_reset = gvm_cpu_synchronize_post_reset;
    ops->synchronize_post_init = gvm_cpu_synchronize_post_init;
    ops->synchronize_state = gvm_cpu_synchronize_state;
    ops->synchronize_pre_loadvm = gvm_cpu_synchronize_pre_loadvm;
    ops->kick_vcpu_thread = gvm_kick_vcpu_thread;
}

static const TypeInfo gvm_accel_ops_type = {
    .name = ACCEL_OPS_NAME("gvm"),

    .parent = TYPE_ACCEL_OPS,
    .class_init = gvm_accel_ops_class_init,
    .abstract = true,
};

static void gvm_accel_ops_register_types(void)
{
    type_register_static(&gvm_accel_ops_type);
}
type_init(gvm_accel_ops_register_types);
