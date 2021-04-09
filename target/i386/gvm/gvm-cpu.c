/*
 * x86 GVM CPU type initialization
 *
 * Copyright 2021 SUSE LLC
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "host-cpu.h"
#include "gvm-cpu.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"

#include "gvm_i386.h"
#include "hw/core/accel-cpu.h"

static bool gvm_cpu_realizefn(CPUState *cs, Error **errp)
{
    /*
     * The realize order is important, since x86_cpu_realize() checks if
     * nothing else has been set by the user (or by accelerators) in
     * cpu->ucode_rev and cpu->phys_bits, and updates the CPUID results in
     * mwait.ecx.
     * This accel realization code also assumes cpu features are already expanded.
     *
     * realize order:
     *
     * x86_cpu_realize():
     *  -> x86_cpu_expand_features()
     *  -> cpu_exec_realizefn():
     *            -> accel_cpu_realizefn()
     *               gvm_cpu_realizefn() -> host_cpu_realizefn()
     *  -> check/update ucode_rev, phys_bits, mwait
     */
    return host_cpu_realizefn(cs, errp);
}

static void gvm_cpu_max_instance_init(X86CPU *cpu)
{
    CPUX86State *env = &cpu->env;
    GVMState *s = gvm_state;

    host_cpu_max_instance_init(cpu);

    env->cpuid_min_level =
        gvm_arch_get_supported_cpuid(s, 0x0, 0, R_EAX);
    env->cpuid_min_xlevel =
        gvm_arch_get_supported_cpuid(s, 0x80000000, 0, R_EAX);
    env->cpuid_min_xlevel2 =
        gvm_arch_get_supported_cpuid(s, 0xC0000000, 0, R_EAX);
}

static void gvm_cpu_xsave_init(void)
{
    static bool first = true;
    uint32_t eax, ebx, ecx, edx;
    int i;

    if (!first) {
        return;
    }
    first = false;

    /* x87 and SSE states are in the legacy region of the XSAVE area. */
    x86_ext_save_areas[XSTATE_FP_BIT].offset = 0;
    x86_ext_save_areas[XSTATE_SSE_BIT].offset = 0;

    for (i = XSTATE_SSE_BIT + 1; i < XSAVE_STATE_AREA_COUNT; i++) {
        ExtSaveArea *esa = &x86_ext_save_areas[i];

        if (!esa->size) {
            continue;
        }
        if ((x86_cpu_get_supported_feature_word(esa->feature, false) & esa->bits)
            != esa->bits) {
            continue;
        }
        host_cpuid(0xd, i, &eax, &ebx, &ecx, &edx);
        if (eax != 0) {
            assert(esa->size == eax);
            esa->offset = ebx;
            esa->ecx = ecx;
        }
    }
}

/*
 * GVM-specific features that are automatically added/removed
 * from cpudef models when GVM is enabled.
 * Only for builtin_x86_defs models initialized with x86_register_cpudef_types.
 *
 * NOTE: features can be enabled by default only if they were
 *       already available in the oldest kernel version supported
 *       by the GVM accelerator (see "OS requirements" section at
 *       docs/system/target-i386.rst)
 */
static PropValue gvm_default_props[] = {
    { "x2apic", "on" },
    { "acpi", "off" },
    { "monitor", "off" },
    { "svm", "off" },
    { NULL, NULL },
};

/*
 * Only for builtin_x86_defs models initialized with x86_register_cpudef_types.
 */
void x86_cpu_change_gvm_default(const char *prop, const char *value)
{
    PropValue *pv;
    for (pv = gvm_default_props; pv->prop; pv++) {
        if (!strcmp(pv->prop, prop)) {
            pv->value = value;
            break;
        }
    }

    /*
     * It is valid to call this function only for properties that
     * are already present in the gvm_default_props table.
     */
    assert(pv->prop);
}

static void gvm_cpu_instance_init(CPUState *cs)
{
    X86CPU *cpu = X86_CPU(cs);
    X86CPUClass *xcc = X86_CPU_GET_CLASS(cpu);

    host_cpu_instance_init(cpu);

    if (xcc->model) {
        /* Special cases not set in the X86CPUDefinition structs: */
        x86_cpu_apply_props(cpu, gvm_default_props);
    }

    if (cpu->max_features) {
        gvm_cpu_max_instance_init(cpu);
    }

    gvm_cpu_xsave_init();
}

static void gvm_cpu_accel_class_init(ObjectClass *oc, void *data)
{
    AccelCPUClass *acc = ACCEL_CPU_CLASS(oc);

    acc->cpu_realizefn = gvm_cpu_realizefn;
    acc->cpu_instance_init = gvm_cpu_instance_init;
}
static const TypeInfo gvm_cpu_accel_type_info = {
    .name = ACCEL_CPU_NAME("gvm"),

    .parent = TYPE_ACCEL_CPU,
    .class_init = gvm_cpu_accel_class_init,
    .abstract = true,
};
static void gvm_cpu_accel_register_types(void)
{
    type_register_static(&gvm_cpu_accel_type_info);
}
type_init(gvm_cpu_accel_register_types);
