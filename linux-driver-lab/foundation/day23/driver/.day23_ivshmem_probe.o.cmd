cmd_/home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.o := aarch64-linux-gnu-gcc -Wp,-MMD,/home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/.day23_ivshmem_probe.o.d -nostdinc -isystem /usr/lib/gcc-cross/aarch64-linux-gnu/11/include -I/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include -I./arch/arm64/include/generated -I/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include -I./include -I/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi -I./arch/arm64/include/generated/uapi -I/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi -I./include/generated/uapi -include /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler-version.h -include /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kconfig.h -include /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler_types.h -D__KERNEL__ -mlittle-endian -DCC_USING_PATCHABLE_FUNCTION_ENTRY -DKASAN_SHADOW_SCALE_SHIFT= -fmacro-prefix-map=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/= -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -std=gnu89 -mgeneral-regs-only -DCONFIG_CC_HAS_K_CONSTRAINT=1 -Wno-psabi -mabi=lp64 -fno-asynchronous-unwind-tables -fno-unwind-tables -mbranch-protection=pac-ret+leaf+bti -Wa,-march=armv8.5-a -DARM64_ASM_ARCH='"armv8.5-a"' -DKASAN_SHADOW_SCALE_SHIFT= -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-address-of-packed-member -O2 -fno-allow-store-data-races -Wframe-larger-than=2048 -fstack-protector-strong -Wimplicit-fallthrough=5 -Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-stack-clash-protection -g -fno-var-tracking -femit-struct-debug-baseonly -fpatchable-function-entry=2 -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-stringop-truncation -Wno-zero-length-bounds -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict -Wno-maybe-uninitialized -fno-strict-overflow -fno-stack-check -fconserve-stack -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wno-packed-not-aligned -mstack-protector-guard=sysreg -mstack-protector-guard-reg=sp_el0 -mstack-protector-guard-offset=1112  -DMODULE  -DKBUILD_BASENAME='"day23_ivshmem_probe"' -DKBUILD_MODNAME='"day23_ivshmem_probe"' -D__KBUILD_MODNAME=kmod_day23_ivshmem_probe -c -o /home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.o /home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.c

source_/home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.o := /home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.c

deps_/home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.o := \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler_types.h \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler_attributes.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler-gcc.h \
    $(wildcard include/config/RETPOLINE) \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/KCOV) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/compiler.h \
    $(wildcard include/config/CFI_CLANG) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/init.h \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
    $(wildcard include/config/STRICT_KERNEL_RWX) \
    $(wildcard include/config/STRICT_MODULE_RWX) \
    $(wildcard include/config/LTO_CLANG) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/STACK_VALIDATION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/compiler_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/rwonce.h \
    $(wildcard include/config/LTO) \
    $(wildcard include/config/AS_HAS_LDAPR) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/rwonce.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/types.h \
  arch/arm64/include/generated/uapi/asm/types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/int-ll64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/int-ll64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/bitsperlong.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitsperlong.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/bitsperlong.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/posix_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/stddef.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/stddef.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/posix_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/posix_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/io.h \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/HAS_IOPORT_MAP) \
    $(wildcard include/config/PCI) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/bug.h \
    $(wildcard include/config/GENERIC_BUG) \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/bug.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/stringify.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/asm-bug.h \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/brk-imm.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bug.h \
    $(wildcard include/config/BUG) \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
    $(wildcard include/config/SMP) \
    $(wildcard include/config/MODULES) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/instrumentation.h \
    $(wildcard include/config/DEBUG_ENTRY) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/once_lite.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/panic.h \
    $(wildcard include/config/PANIC_TIMEOUT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/PRINTK_INDEX) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/stdarg.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kern_levels.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
    $(wildcard include/config/MODULE_REL_CRCS) \
    $(wildcard include/config/TRIM_UNUSED_KSYMS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/linkage.h \
    $(wildcard include/config/ARM64_BTI_KERNEL) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/cache.h \
    $(wildcard include/config/ARCH_HAS_CACHE_LINE_SIZE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/kernel.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/sysinfo.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/const.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/const.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/const.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/cache.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/cputype.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/sysreg.h \
    $(wildcard include/config/BROKEN_GAS_INST) \
    $(wildcard include/config/ARM64_PA_BITS_52) \
    $(wildcard include/config/ARM64_4K_PAGES) \
    $(wildcard include/config/ARM64_16K_PAGES) \
    $(wildcard include/config/ARM64_64K_PAGES) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/bits.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/bits.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/build_bug.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kasan-tags.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/alternative.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/alternative-macros.h \
  arch/arm64/include/generated/asm/cpucaps.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/insn-def.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/bitops.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/typecheck.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/bitops.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/builtin-__ffs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/builtin-ffs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/builtin-__fls.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/builtin-fls.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/ffz.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/fls64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/find.h \
    $(wildcard include/config/GENERIC_FIND_FIRST_BIT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/sched.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/hweight.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/arch_hweight.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/const_hweight.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/atomic.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/atomic.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/atomic.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/barrier.h \
    $(wildcard include/config/ARM64_PSEUDO_NMI) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/barrier.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/cmpxchg.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/lse.h \
    $(wildcard include/config/ARM64_LSE_ATOMICS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/atomic_ll_sc.h \
    $(wildcard include/config/CC_HAS_K_CONSTRAINT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/jump_label.h \
    $(wildcard include/config/JUMP_LABEL) \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/jump_label.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/insn.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/atomic_lse.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/atomic/atomic-arch-fallback.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/atomic/atomic-long.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/atomic/atomic-instrumented.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/instrumented.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/instrumented-atomic.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/lock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/instrumented-lock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/non-atomic.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/le.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/byteorder.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/byteorder/little_endian.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/byteorder/little_endian.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/swab.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/swab.h \
  arch/arm64/include/generated/uapi/asm/swab.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/swab.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/byteorder/generic.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ratelimit_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/param.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/param.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/param.h \
    $(wildcard include/config/HZ) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/param.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/spinlock_types.h \
    $(wildcard include/config/PREEMPT_RT) \
    $(wildcard include/config/DEBUG_LOCK_ALLOC) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/spinlock_types_raw.h \
    $(wildcard include/config/DEBUG_SPINLOCK) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/spinlock_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/NR_CPUS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/qrwlock_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/lockdep_types.h \
    $(wildcard include/config/PROVE_RAW_LOCK_NESTING) \
    $(wildcard include/config/PREEMPT_LOCK) \
    $(wildcard include/config/LOCKDEP) \
    $(wildcard include/config/LOCK_STAT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rwlock_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/err.h \
  arch/arm64/include/generated/uapi/asm/errno.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/errno.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/errno-base.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/io.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pgtable.h \
    $(wildcard include/config/PGTABLE_LEVELS) \
    $(wildcard include/config/HIGHPTE) \
    $(wildcard include/config/TRANSPARENT_HUGEPAGE) \
    $(wildcard include/config/GUP_GET_PTE_LOW_HIGH) \
    $(wildcard include/config/HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD) \
    $(wildcard include/config/HAVE_ARCH_SOFT_DIRTY) \
    $(wildcard include/config/ARCH_ENABLE_THP_MIGRATION) \
    $(wildcard include/config/ARCH_HAS_PTE_DEVMAP) \
    $(wildcard include/config/NUMA_BALANCING) \
    $(wildcard include/config/HAVE_ARCH_HUGE_VMAP) \
    $(wildcard include/config/X86_ESPFIX64) \
    $(wildcard include/config/HUGETLB_PAGE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pfn.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/pgtable.h \
    $(wildcard include/config/DEBUG_VM) \
    $(wildcard include/config/ARM64_MTE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/proc-fns.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/page.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/page-def.h \
    $(wildcard include/config/ARM64_PAGE_SHIFT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/personality.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/personality.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/pgtable-types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/pgtable-nop4d.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/memory.h \
    $(wildcard include/config/ARM64_VA_BITS) \
    $(wildcard include/config/KASAN_SHADOW_OFFSET) \
    $(wildcard include/config/KASAN) \
    $(wildcard include/config/VMAP_STACK) \
    $(wildcard include/config/DEBUG_VIRTUAL) \
    $(wildcard include/config/EFI) \
    $(wildcard include/config/ARM_GIC_V3_ITS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sizes.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mmdebug.h \
    $(wildcard include/config/DEBUG_VM_PGFLAGS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/memory_model.h \
    $(wildcard include/config/FLATMEM) \
    $(wildcard include/config/SPARSEMEM_VMEMMAP) \
    $(wildcard include/config/SPARSEMEM) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/getorder.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/mte.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/mte-def.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/bitfield.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/page-flags.h \
    $(wildcard include/config/ARCH_USES_PG_UNCACHED) \
    $(wildcard include/config/MEMORY_FAILURE) \
    $(wildcard include/config/PAGE_IDLE_FLAG) \
    $(wildcard include/config/HIGHMEM) \
    $(wildcard include/config/SWAP) \
    $(wildcard include/config/THP_SWAP) \
    $(wildcard include/config/KSM) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mm_types.h \
    $(wildcard include/config/HAVE_ALIGNED_STRUCT_PAGE) \
    $(wildcard include/config/MEMCG) \
    $(wildcard include/config/USERFAULTFD) \
    $(wildcard include/config/NUMA) \
    $(wildcard include/config/HAVE_ARCH_COMPAT_MMAP_BASES) \
    $(wildcard include/config/MEMBARRIER) \
    $(wildcard include/config/AIO) \
    $(wildcard include/config/MMU_NOTIFIER) \
    $(wildcard include/config/ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH) \
    $(wildcard include/config/IOMMU_SUPPORT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mm_types_task.h \
    $(wildcard include/config/SPLIT_PTLOCK_CPUS) \
    $(wildcard include/config/ARCH_ENABLE_SPLIT_PMD_PTLOCK) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/threads.h \
    $(wildcard include/config/BASE_SMALL) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/cpumask.h \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/PROVE_LOCKING) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/FTRACE_MCOUNT_RECORD) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/align.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/limits.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/limits.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/limits.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kstrtox.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/math.h \
  arch/arm64/include/generated/asm/div64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/div64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/minmax.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
    $(wildcard include/config/HAVE_STATIC_CALL_INLINE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/bitmap.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/errno.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/errno.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/string.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/string.h \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/auxvec.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/auxvec.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/auxvec.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/list.h \
    $(wildcard include/config/DEBUG_LIST) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/poison.h \
    $(wildcard include/config/ILLEGAL_POINTER_VALUE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/spinlock.h \
    $(wildcard include/config/PREEMPTION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/preempt.h \
    $(wildcard include/config/PREEMPT_COUNT) \
    $(wildcard include/config/DEBUG_PREEMPT) \
    $(wildcard include/config/TRACE_PREEMPT_TOGGLE) \
    $(wildcard include/config/PREEMPT_NOTIFIERS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/preempt.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/thread_info.h \
    $(wildcard include/config/THREAD_INFO_IN_TASK) \
    $(wildcard include/config/GENERIC_ENTRY) \
    $(wildcard include/config/HAVE_ARCH_WITHIN_STACK_FRAMES) \
    $(wildcard include/config/HARDENED_USERCOPY) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/restart_block.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/time64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/math64.h \
    $(wildcard include/config/ARCH_SUPPORTS_INT128) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/math64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/time64.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/time.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/time_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/current.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/thread_info.h \
    $(wildcard include/config/ARM64_SW_TTBR0_PAN) \
    $(wildcard include/config/SHADOW_CALL_STACK) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/stack_pointer.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/irqflags.h \
    $(wildcard include/config/TRACE_IRQFLAGS) \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/irqflags.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/ptrace.h \
    $(wildcard include/config/COMPAT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/cpufeature.h \
    $(wildcard include/config/ARM64_PAN) \
    $(wildcard include/config/ARM64_SVE) \
    $(wildcard include/config/ARM64_CNP) \
    $(wildcard include/config/ARM64_PTR_AUTH) \
    $(wildcard include/config/ARM64_DEBUG_PRIORITY_MASKING) \
    $(wildcard include/config/ARM64_BTI) \
    $(wildcard include/config/ARM64_TLB_RANGE) \
    $(wildcard include/config/ARM64_PA_BITS) \
    $(wildcard include/config/ARM64_HW_AFDBM) \
    $(wildcard include/config/ARM64_AMU_EXTN) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/hwcap.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/hwcap.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/ptrace.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/sve_context.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/percpu.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/percpu.h \
    $(wildcard include/config/HAVE_SETUP_PER_CPU_AREA) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/percpu-defs.h \
    $(wildcard include/config/DEBUG_FORCE_WEAK_PER_CPU) \
    $(wildcard include/config/AMD_MEM_ENCRYPT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/bottom_half.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/lockdep.h \
    $(wildcard include/config/DEBUG_LOCKING_API_SELFTESTS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/smp.h \
    $(wildcard include/config/UP_LATE_INIT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/smp_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/llist.h \
    $(wildcard include/config/ARCH_HAVE_NMI_SAFE_CMPXCHG) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/smp.h \
    $(wildcard include/config/ARM64_ACPI_PARKING_PROTOCOL) \
  arch/arm64/include/generated/asm/mmiowb.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/mmiowb.h \
    $(wildcard include/config/MMIOWB) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/spinlock.h \
  arch/arm64/include/generated/asm/qspinlock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/qspinlock.h \
  arch/arm64/include/generated/asm/qrwlock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/qrwlock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/processor.h \
    $(wildcard include/config/KUSER_HELPERS) \
    $(wildcard include/config/ARM64_FORCE_52BIT) \
    $(wildcard include/config/HAVE_HW_BREAKPOINT) \
    $(wildcard include/config/ARM64_PTR_AUTH_KERNEL) \
    $(wildcard include/config/ARM64_TAGGED_ADDR_ABI) \
    $(wildcard include/config/GCC_PLUGIN_STACKLEAK) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/processor.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/vdso/processor.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/hw_breakpoint.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/virt.h \
    $(wildcard include/config/KVM) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/sections.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/sections.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/kasan.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/mte-kasan.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/pgtable-hwdef.h \
    $(wildcard include/config/ARM64_CONT_PTE_SHIFT) \
    $(wildcard include/config/ARM64_CONT_PMD_SHIFT) \
    $(wildcard include/config/ARM64_VA_BITS_52) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/pointer_auth.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/prctl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/random.h \
    $(wildcard include/config/ARCH_RANDOM) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/once.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/random.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/ioctl.h \
  arch/arm64/include/generated/uapi/asm/ioctl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/ioctl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/ioctl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/irqnr.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/irqnr.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/prandom.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/percpu.h \
    $(wildcard include/config/NEED_PER_CPU_EMBED_FIRST_CHUNK) \
    $(wildcard include/config/NEED_PER_CPU_PAGE_FIRST_CHUNK) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/archrandom.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/arm-smccc.h \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/HAVE_ARM_SMCCC) \
    $(wildcard include/config/ARM) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/spectre.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/fpsimd.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/sigcontext.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rwlock.h \
    $(wildcard include/config/PREEMPT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/INLINE_SPIN_LOCK) \
    $(wildcard include/config/INLINE_SPIN_LOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK_BH) \
    $(wildcard include/config/UNINLINE_SPIN_UNLOCK) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/GENERIC_LOCKBREAK) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/INLINE_READ_LOCK) \
    $(wildcard include/config/INLINE_WRITE_LOCK) \
    $(wildcard include/config/INLINE_READ_LOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_LOCK_BH) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_READ_TRYLOCK) \
    $(wildcard include/config/INLINE_WRITE_TRYLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_BH) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQRESTORE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rbtree.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rbtree_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rcupdate.h \
    $(wildcard include/config/PREEMPT_RCU) \
    $(wildcard include/config/TINY_RCU) \
    $(wildcard include/config/TASKS_RCU_GENERIC) \
    $(wildcard include/config/RCU_STALL_COMMON) \
    $(wildcard include/config/NO_HZ_FULL) \
    $(wildcard include/config/RCU_NOCB_CPU) \
    $(wildcard include/config/TASKS_RCU) \
    $(wildcard include/config/TASKS_TRACE_RCU) \
    $(wildcard include/config/TASKS_RUDE_RCU) \
    $(wildcard include/config/TREE_RCU) \
    $(wildcard include/config/DEBUG_OBJECTS_RCU_HEAD) \
    $(wildcard include/config/PROVE_RCU) \
    $(wildcard include/config/ARCH_WEAK_RELEASE_ACQUIRE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rcutree.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rwsem.h \
    $(wildcard include/config/RWSEM_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_RWSEMS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/osq_lock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/completion.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/swait.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/wait.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/wait.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/uprobes.h \
    $(wildcard include/config/UPROBES) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/uprobes.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/debug-monitors.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/esr.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/probes.h \
    $(wildcard include/config/KPROBES) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/page-flags-layout.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/numa.h \
    $(wildcard include/config/NODES_SHIFT) \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/sparsemem.h \
  include/generated/bounds.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/workqueue.h \
    $(wildcard include/config/DEBUG_OBJECTS_WORK) \
    $(wildcard include/config/FREEZER) \
    $(wildcard include/config/SYSFS) \
    $(wildcard include/config/WQ_WATCHDOG) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/timer.h \
    $(wildcard include/config/DEBUG_OBJECTS_TIMERS) \
    $(wildcard include/config/NO_HZ_COMMON) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ktime.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/time.h \
    $(wildcard include/config/POSIX_TIMERS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/time32.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/timex.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/timex.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/timex.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER_OOL_WORKAROUND) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/clocksource/arm_arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/timecounter.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/timex.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/time32.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/time.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/jiffies.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/jiffies.h \
  include/generated/timeconst.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/vdso/ktime.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/timekeeping.h \
    $(wildcard include/config/GENERIC_CMOS_UPDATE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/clocksource_ids.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/debugobjects.h \
    $(wildcard include/config/DEBUG_OBJECTS) \
    $(wildcard include/config/DEBUG_OBJECTS_FREE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/seqlock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mutex.h \
    $(wildcard include/config/MUTEX_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_MUTEXES) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/debug_locks.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ww_mutex.h \
    $(wildcard include/config/DEBUG_RT_MUTEXES) \
    $(wildcard include/config/DEBUG_WW_MUTEX_SLOWPATH) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rtmutex.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/mmu.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/refcount.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/pgtable-prot.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/tlbflush.h \
    $(wildcard include/config/ARM64_WORKAROUND_REPEAT_TLBI) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched.h \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_NATIVE) \
    $(wildcard include/config/SCHED_INFO) \
    $(wildcard include/config/SCHEDSTATS) \
    $(wildcard include/config/FAIR_GROUP_SCHED) \
    $(wildcard include/config/RT_GROUP_SCHED) \
    $(wildcard include/config/RT_MUTEXES) \
    $(wildcard include/config/UCLAMP_TASK) \
    $(wildcard include/config/UCLAMP_BUCKETS_COUNT) \
    $(wildcard include/config/KMAP_LOCAL) \
    $(wildcard include/config/SCHED_CORE) \
    $(wildcard include/config/CGROUP_SCHED) \
    $(wildcard include/config/BLK_DEV_IO_TRACE) \
    $(wildcard include/config/PSI) \
    $(wildcard include/config/COMPAT_BRK) \
    $(wildcard include/config/CGROUPS) \
    $(wildcard include/config/BLK_CGROUP) \
    $(wildcard include/config/PAGE_OWNER) \
    $(wildcard include/config/EVENTFD) \
    $(wildcard include/config/STACKPROTECTOR) \
    $(wildcard include/config/ARCH_HAS_SCALED_CPUTIME) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_GEN) \
    $(wildcard include/config/POSIX_CPUTIMERS) \
    $(wildcard include/config/POSIX_CPU_TIMERS_TASK_WORK) \
    $(wildcard include/config/KEYS) \
    $(wildcard include/config/SYSVIPC) \
    $(wildcard include/config/DETECT_HUNG_TASK) \
    $(wildcard include/config/IO_URING) \
    $(wildcard include/config/AUDIT) \
    $(wildcard include/config/AUDITSYSCALL) \
    $(wildcard include/config/UBSAN) \
    $(wildcard include/config/UBSAN_TRAP) \
    $(wildcard include/config/BLOCK) \
    $(wildcard include/config/COMPACTION) \
    $(wildcard include/config/TASK_XACCT) \
    $(wildcard include/config/CPUSETS) \
    $(wildcard include/config/X86_CPU_RESCTRL) \
    $(wildcard include/config/FUTEX) \
    $(wildcard include/config/PERF_EVENTS) \
    $(wildcard include/config/RSEQ) \
    $(wildcard include/config/TASK_DELAY_ACCT) \
    $(wildcard include/config/FAULT_INJECTION) \
    $(wildcard include/config/LATENCYTOP) \
    $(wildcard include/config/KUNIT) \
    $(wildcard include/config/FUNCTION_GRAPH_TRACER) \
    $(wildcard include/config/BCACHE) \
    $(wildcard include/config/LIVEPATCH) \
    $(wildcard include/config/SECURITY) \
    $(wildcard include/config/BPF_SYSCALL) \
    $(wildcard include/config/X86_MCE) \
    $(wildcard include/config/KRETPROBES) \
    $(wildcard include/config/ARCH_HAS_PARANOID_L1D_FLUSH) \
    $(wildcard include/config/ARCH_TASK_STRUCT_ON_STACK) \
    $(wildcard include/config/DEBUG_RSEQ) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/sched.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pid.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rculist.h \
    $(wildcard include/config/PROVE_RCU_LIST) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sem.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/sem.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ipc.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/uidgid.h \
    $(wildcard include/config/MULTIUSER) \
    $(wildcard include/config/USER_NS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/highuid.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rhashtable-types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/ipc.h \
  arch/arm64/include/generated/uapi/asm/ipcbuf.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/ipcbuf.h \
  arch/arm64/include/generated/uapi/asm/sembuf.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/sembuf.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/shm.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/shm.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/hugetlb_encode.h \
  arch/arm64/include/generated/uapi/asm/shmbuf.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/shmbuf.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/shmparam.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/shmparam.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/plist.h \
    $(wildcard include/config/DEBUG_PLIST) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/hrtimer.h \
    $(wildcard include/config/HIGH_RES_TIMERS) \
    $(wildcard include/config/TIME_LOW_RES) \
    $(wildcard include/config/TIMERFD) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/hrtimer_defs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/timerqueue.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/seccomp.h \
    $(wildcard include/config/SECCOMP) \
    $(wildcard include/config/HAVE_ARCH_SECCOMP_FILTER) \
    $(wildcard include/config/SECCOMP_FILTER) \
    $(wildcard include/config/CHECKPOINT_RESTORE) \
    $(wildcard include/config/SECCOMP_CACHE_DEBUG) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/seccomp.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/seccomp.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/unistd.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/unistd.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/unistd.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/seccomp.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/unistd.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/nodemask.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/resource.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/resource.h \
  arch/arm64/include/generated/uapi/asm/resource.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/resource.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/resource.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/latencytop.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/prio.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/signal_types.h \
    $(wildcard include/config/OLD_SIGACTION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/signal.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/signal.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/signal.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/signal.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/signal.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/signal-defs.h \
  arch/arm64/include/generated/uapi/asm/siginfo.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/siginfo.h \
  arch/arm64/include/generated/uapi/asm/siginfo.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/syscall_user_dispatch.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/task_io_accounting.h \
    $(wildcard include/config/TASK_IO_ACCOUNTING) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/posix-timers.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/alarmtimer.h \
    $(wildcard include/config/RTC_CLASS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/task_work.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/rseq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kcsan.h \
  arch/arm64/include/generated/asm/kmap_size.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/kmap_size.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/fixmap.h \
    $(wildcard include/config/ACPI_APEI_GHES) \
    $(wildcard include/config/ARM_SDE_INTERFACE) \
    $(wildcard include/config/UNMAP_KERNEL_AT_EL0) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/boot.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/fixmap.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/pgtable_uffd.h \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_WP) \
  arch/arm64/include/generated/asm/early_ioremap.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/early_ioremap.h \
    $(wildcard include/config/GENERIC_EARLY_IOREMAP) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/io.h \
    $(wildcard include/config/GENERIC_IOMAP) \
    $(wildcard include/config/GENERIC_IOREMAP) \
    $(wildcard include/config/VIRT_TO_BUS) \
    $(wildcard include/config/GENERIC_DEVMEM_IS_ALLOWED) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/pci_iomap.h \
    $(wildcard include/config/NO_GENERIC_PCI_IOPORT_MAP) \
    $(wildcard include/config/GENERIC_PCI_IOMAP) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/logic_pio.h \
    $(wildcard include/config/INDIRECT_PIO) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/fwnode.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/vmalloc.h \
    $(wildcard include/config/KASAN_VMALLOC) \
    $(wildcard include/config/HAVE_ARCH_HUGE_VMALLOC) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/overflow.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/vmalloc.h \
    $(wildcard include/config/PTDUMP_DEBUGFS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/module.h \
    $(wildcard include/config/MODULES_TREE_LOOKUP) \
    $(wildcard include/config/STACKTRACE_BUILD_ID) \
    $(wildcard include/config/MODULE_SIG) \
    $(wildcard include/config/KALLSYMS) \
    $(wildcard include/config/TRACEPOINTS) \
    $(wildcard include/config/TREE_SRCU) \
    $(wildcard include/config/BPF_EVENTS) \
    $(wildcard include/config/DEBUG_INFO_BTF_MODULES) \
    $(wildcard include/config/EVENT_TRACING) \
    $(wildcard include/config/MODULE_UNLOAD) \
    $(wildcard include/config/CONSTRUCTORS) \
    $(wildcard include/config/FUNCTION_ERROR_INJECTION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/stat.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/stat.h \
  arch/arm64/include/generated/uapi/asm/stat.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/stat.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/compat.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/compat.h \
    $(wildcard include/config/COMPAT_FOR_U64_ALIGNMENT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/task_stack.h \
    $(wildcard include/config/STACK_GROWSUP) \
    $(wildcard include/config/DEBUG_STACK_USAGE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/magic.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/stat.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/buildid.h \
    $(wildcard include/config/CRASH_CORE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kmod.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/umh.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/gfp.h \
    $(wildcard include/config/ZONE_DMA) \
    $(wildcard include/config/ZONE_DMA32) \
    $(wildcard include/config/ZONE_DEVICE) \
    $(wildcard include/config/PM_SLEEP) \
    $(wildcard include/config/CONTIG_ALLOC) \
    $(wildcard include/config/CMA) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mmzone.h \
    $(wildcard include/config/FORCE_MAX_ZONEORDER) \
    $(wildcard include/config/MEMORY_ISOLATION) \
    $(wildcard include/config/ZSMALLOC) \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/PAGE_EXTENSION) \
    $(wildcard include/config/DEFERRED_STRUCT_PAGE_INIT) \
    $(wildcard include/config/HAVE_MEMORYLESS_NODES) \
    $(wildcard include/config/SPARSEMEM_EXTREME) \
    $(wildcard include/config/HAVE_ARCH_PFN_VALID) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pageblock-flags.h \
    $(wildcard include/config/HUGETLB_PAGE_SIZE_VARIABLE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/local_lock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/local_lock_internal.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/memory_hotplug.h \
    $(wildcard include/config/ARCH_HAS_ADD_PAGES) \
    $(wildcard include/config/HAVE_ARCH_NODEDATA_EXTENSION) \
    $(wildcard include/config/MEMORY_HOTREMOVE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/notifier.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/srcu.h \
    $(wildcard include/config/TINY_SRCU) \
    $(wildcard include/config/SRCU) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rcu_segcblist.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/srcutree.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rcu_node_tree.h \
    $(wildcard include/config/RCU_FANOUT) \
    $(wildcard include/config/RCU_FANOUT_LEAF) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/mmzone.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/numa.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/topology.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/arch_topology.h \
    $(wildcard include/config/GENERIC_ARCH_TOPOLOGY) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/topology.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/numa.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/topology.h \
    $(wildcard include/config/USE_PERCPU_NUMA_NODE_ID) \
    $(wildcard include/config/SCHED_SMT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sysctl.h \
    $(wildcard include/config/SYSCTL) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/sysctl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/elf.h \
    $(wildcard include/config/ARCH_USE_GNU_PROPERTY) \
    $(wildcard include/config/ARCH_HAVE_ELF_PROT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/elf.h \
    $(wildcard include/config/COMPAT_VDSO) \
  arch/arm64/include/generated/asm/user.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/user.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/elf.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/elf-em.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/fs.h \
    $(wildcard include/config/READ_ONLY_THP_FOR_FS) \
    $(wildcard include/config/FS_POSIX_ACL) \
    $(wildcard include/config/CGROUP_WRITEBACK) \
    $(wildcard include/config/IMA) \
    $(wildcard include/config/FILE_LOCKING) \
    $(wildcard include/config/FSNOTIFY) \
    $(wildcard include/config/FS_ENCRYPTION) \
    $(wildcard include/config/FS_VERITY) \
    $(wildcard include/config/EPOLL) \
    $(wildcard include/config/UNICODE) \
    $(wildcard include/config/QUOTA) \
    $(wildcard include/config/FS_DAX) \
    $(wildcard include/config/MIGRATION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/wait_bit.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kdev_t.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/kdev_t.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dcache.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rculist_bl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/list_bl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/bit_spinlock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/lockref.h \
    $(wildcard include/config/ARCH_USE_CMPXCHG_LOCKREF) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/stringhash.h \
    $(wildcard include/config/DCACHE_WORD_ACCESS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/hash.h \
    $(wildcard include/config/HAVE_ARCH_HASH) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/path.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/list_lru.h \
    $(wildcard include/config/MEMCG_KMEM) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/shrinker.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/radix-tree.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/xarray.h \
    $(wildcard include/config/XARRAY_MULTI) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kconfig.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/capability.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/capability.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/semaphore.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/fcntl.h \
    $(wildcard include/config/ARCH_32BIT_OFF_T) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/fcntl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/uapi/asm/fcntl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/asm-generic/fcntl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/openat2.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/migrate_mode.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/percpu-rwsem.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rcuwait.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/signal.h \
    $(wildcard include/config/SCHED_AUTOGROUP) \
    $(wildcard include/config/BSD_PROCESS_ACCT) \
    $(wildcard include/config/TASKSTATS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/signal.h \
    $(wildcard include/config/PROC_FS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/jobctl.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/task.h \
    $(wildcard include/config/HAVE_EXIT_THREAD) \
    $(wildcard include/config/ARCH_WANTS_DYNAMIC_TASK_STRUCT) \
    $(wildcard include/config/HAVE_ARCH_THREAD_STRUCT_WHITELIST) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/uaccess.h \
    $(wildcard include/config/SET_FS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/fault-inject-usercopy.h \
    $(wildcard include/config/FAULT_INJECTION_USERCOPY) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/uaccess.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/kernel-pgtable.h \
    $(wildcard include/config/RANDOMIZE_BASE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/extable.h \
    $(wildcard include/config/BPF_JIT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/cred.h \
    $(wildcard include/config/DEBUG_CREDENTIALS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/key.h \
    $(wildcard include/config/KEY_NOTIFICATIONS) \
    $(wildcard include/config/NET) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/assoc_array.h \
    $(wildcard include/config/ASSOCIATIVE_ARRAY) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/user.h \
    $(wildcard include/config/WATCH_QUEUE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/percpu_counter.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ratelimit.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rcu_sync.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/delayed_call.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/uuid.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/uuid.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/errseq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ioprio.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/rt.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/iocontext.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/ioprio.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/fs_types.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mount.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/fs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/quota.h \
    $(wildcard include/config/QUOTA_NETLINK_INTERFACE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/dqblk_xfs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dqblk_v1.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dqblk_v2.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dqblk_qtree.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/projid.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/quota.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/nfs_fs_i.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kobject.h \
    $(wildcard include/config/UEVENT_HELPER) \
    $(wildcard include/config/DEBUG_KOBJECT_RELEASE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sysfs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kernfs.h \
    $(wildcard include/config/KERNFS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/idr.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kobject_ns.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kref.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/moduleparam.h \
    $(wildcard include/config/ALPHA) \
    $(wildcard include/config/IA64) \
    $(wildcard include/config/PPC64) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/rbtree_latch.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/error-injection.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/error-injection.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/tracepoint-defs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/static_key.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/cfi.h \
    $(wildcard include/config/CFI_CLANG_SHADOW) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/module.h \
    $(wildcard include/config/ARM64_MODULE_PLTS) \
    $(wildcard include/config/DYNAMIC_FTRACE) \
    $(wildcard include/config/ARM64_ERRATUM_843419) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/module.h \
    $(wildcard include/config/HAVE_MOD_ARCH_SPECIFIC) \
    $(wildcard include/config/MODULES_USE_ELF_REL) \
    $(wildcard include/config/MODULES_USE_ELF_RELA) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pci.h \
    $(wildcard include/config/PCI_IOV) \
    $(wildcard include/config/PCIEAER) \
    $(wildcard include/config/PCIEPORTBUS) \
    $(wildcard include/config/PCIEASPM) \
    $(wildcard include/config/HOTPLUG_PCI_PCIE) \
    $(wildcard include/config/PCIE_PTM) \
    $(wildcard include/config/PCI_MSI) \
    $(wildcard include/config/PCIE_DPC) \
    $(wildcard include/config/PCI_ATS) \
    $(wildcard include/config/PCI_PRI) \
    $(wildcard include/config/PCI_PASID) \
    $(wildcard include/config/PCI_P2PDMA) \
    $(wildcard include/config/PCI_DOMAINS_GENERIC) \
    $(wildcard include/config/PCI_DOMAINS) \
    $(wildcard include/config/ACPI) \
    $(wildcard include/config/PCI_QUIRKS) \
    $(wildcard include/config/PCI_MMCONFIG) \
    $(wildcard include/config/ACPI_MCFG) \
    $(wildcard include/config/HOTPLUG_PCI) \
    $(wildcard include/config/OF) \
    $(wildcard include/config/EEH) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mod_devicetable.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ioport.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/device.h \
    $(wildcard include/config/ENERGY_MODEL) \
    $(wildcard include/config/GENERIC_MSI_IRQ_DOMAIN) \
    $(wildcard include/config/PINCTRL) \
    $(wildcard include/config/GENERIC_MSI_IRQ) \
    $(wildcard include/config/DMA_OPS) \
    $(wildcard include/config/DMA_DECLARE_COHERENT) \
    $(wildcard include/config/DMA_CMA) \
    $(wildcard include/config/SWIOTLB) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_DEVICE) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_CPU) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_CPU_ALL) \
    $(wildcard include/config/DMA_OPS_BYPASS) \
    $(wildcard include/config/DEVTMPFS) \
    $(wildcard include/config/SYSFS_DEPRECATED) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dev_printk.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/energy_model.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/cpufreq.h \
    $(wildcard include/config/CPU_FREQ) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/topology.h \
    $(wildcard include/config/SCHED_DEBUG) \
    $(wildcard include/config/SCHED_MC) \
    $(wildcard include/config/CPU_FREQ_GOV_SCHEDUTIL) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/idle.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/sd_flags.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/klist.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pm.h \
    $(wildcard include/config/VT_CONSOLE_SLEEP) \
    $(wildcard include/config/PM) \
    $(wildcard include/config/PM_CLK) \
    $(wildcard include/config/PM_GENERIC_DOMAINS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/device/bus.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/device/class.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/device/driver.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/device.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pm_wakeup.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/interrupt.h \
    $(wildcard include/config/IRQ_FORCED_THREADING) \
    $(wildcard include/config/GENERIC_IRQ_PROBE) \
    $(wildcard include/config/IRQ_TIMINGS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/irqreturn.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/hardirq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/context_tracking_state.h \
    $(wildcard include/config/CONTEXT_TRACKING) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/ftrace_irq.h \
    $(wildcard include/config/HWLAT_TRACER) \
    $(wildcard include/config/OSNOISE_TRACER) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/vtime.h \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING) \
    $(wildcard include/config/IRQ_TIME_ACCOUNTING) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/hardirq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/irq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/irq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/kvm_arm.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/hardirq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/irq.h \
    $(wildcard include/config/GENERIC_IRQ_EFFECTIVE_AFF_MASK) \
    $(wildcard include/config/GENERIC_IRQ_IPI) \
    $(wildcard include/config/IRQ_DOMAIN_HIERARCHY) \
    $(wildcard include/config/GENERIC_IRQ_MIGRATION) \
    $(wildcard include/config/GENERIC_PENDING_IRQ) \
    $(wildcard include/config/HARDIRQS_SW_RESEND) \
    $(wildcard include/config/GENERIC_IRQ_LEGACY) \
    $(wildcard include/config/GENERIC_IRQ_MULTI_HANDLER) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/irqhandler.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/slab.h \
    $(wildcard include/config/DEBUG_SLAB) \
    $(wildcard include/config/FAILSLAB) \
    $(wildcard include/config/HAVE_HARDENED_USERCOPY_ALLOCATOR) \
    $(wildcard include/config/SLAB) \
    $(wildcard include/config/SLUB) \
    $(wildcard include/config/SLOB) \
    $(wildcard include/config/CC_IS_GCC) \
    $(wildcard include/config/CLANG_VERSION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/percpu-refcount.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/kasan.h \
    $(wildcard include/config/KASAN_STACK) \
    $(wildcard include/config/KASAN_INLINE) \
  arch/arm64/include/generated/asm/irq_regs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/irq_regs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/irqdesc.h \
    $(wildcard include/config/GENERIC_IRQ_DEBUGFS) \
    $(wildcard include/config/SPARSE_IRQ) \
    $(wildcard include/config/IRQ_DOMAIN) \
    $(wildcard include/config/HANDLE_DOMAIN_IRQ) \
  arch/arm64/include/generated/asm/hw_irq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/asm-generic/hw_irq.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/resource_ext.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/pci.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/uapi/linux/pci_regs.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pci_ids.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dmapool.h \
    $(wildcard include/config/HAS_DMA) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/scatterlist.h \
    $(wildcard include/config/NEED_SG_DMA_LENGTH) \
    $(wildcard include/config/DEBUG_SG) \
    $(wildcard include/config/SGL_ALLOC) \
    $(wildcard include/config/ARCH_NO_SG_CHAIN) \
    $(wildcard include/config/SG_POOL) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mm.h \
    $(wildcard include/config/HAVE_ARCH_MMAP_RND_BITS) \
    $(wildcard include/config/HAVE_ARCH_MMAP_RND_COMPAT_BITS) \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/MEM_SOFT_DIRTY) \
    $(wildcard include/config/ARCH_USES_HIGH_VMA_FLAGS) \
    $(wildcard include/config/ARCH_HAS_PKEYS) \
    $(wildcard include/config/PPC) \
    $(wildcard include/config/X86) \
    $(wildcard include/config/PARISC) \
    $(wildcard include/config/SPARC64) \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_MINOR) \
    $(wildcard include/config/SHMEM) \
    $(wildcard include/config/DEV_PAGEMAP_OPS) \
    $(wildcard include/config/DEVICE_PRIVATE) \
    $(wildcard include/config/ARCH_HAS_PTE_SPECIAL) \
    $(wildcard include/config/DEBUG_VM_RB) \
    $(wildcard include/config/PAGE_POISONING) \
    $(wildcard include/config/INIT_ON_ALLOC_DEFAULT_ON) \
    $(wildcard include/config/INIT_ON_FREE_DEFAULT_ON) \
    $(wildcard include/config/DEBUG_PAGEALLOC) \
    $(wildcard include/config/HUGETLBFS) \
    $(wildcard include/config/MAPPING_DIRTY_HELPERS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mmap_lock.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/range.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/page_ext.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/stacktrace.h \
    $(wildcard include/config/STACKTRACE) \
    $(wildcard include/config/ARCH_STACKWALK) \
    $(wildcard include/config/HAVE_RELIABLE_STACKTRACE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/stackdepot.h \
    $(wildcard include/config/STACKDEPOT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/page_ref.h \
    $(wildcard include/config/DEBUG_PAGE_REF) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/memremap.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/huge_mm.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/sched/coredump.h \
    $(wildcard include/config/CORE_DUMP_DEFAULT_ELF_HEADERS) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/vmstat.h \
    $(wildcard include/config/VM_EVENT_COUNTERS) \
    $(wildcard include/config/DEBUG_TLBFLUSH) \
    $(wildcard include/config/DEBUG_VM_VMACACHE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/vm_event_item.h \
    $(wildcard include/config/MEMORY_BALLOON) \
    $(wildcard include/config/BALLOON_COMPACTION) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/arch/arm64/include/asm/pci.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dma-mapping.h \
    $(wildcard include/config/DMA_API_DEBUG) \
    $(wildcard include/config/NEED_DMA_MAP_STATE) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/dma-direction.h \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/mem_encrypt.h \
    $(wildcard include/config/ARCH_HAS_MEM_ENCRYPT) \
  /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/include/linux/pci-dma-compat.h \
  /home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/include/day23_ivshmem_probe.h \

/home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.o: $(deps_/home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.o)

$(deps_/home/wq7/workspace/driver-lab/linux-driver-lab/day23/driver/day23_ivshmem_probe.o):
