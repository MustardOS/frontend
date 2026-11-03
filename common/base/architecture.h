#pragma once

#if defined(__aarch64__)
#define MUOS_ARCHITECTURE "aarch64"
#elif defined(__arm__)
#define MUOS_ARCHITECTURE "armhf"
#elif defined(__x86_64__)
#define MUOS_ARCHITECTURE "x86_64"
#elif defined(__i386__)
#define MUOS_ARCHITECTURE "x86"
#elif defined(__riscv) && __riscv_xlen == 64
#define MUOS_ARCHITECTURE "riscv64"
#else
#define MUOS_ARCHITECTURE "unknown"
#endif
