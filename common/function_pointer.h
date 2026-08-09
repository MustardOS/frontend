#pragma once

#include <string.h>

#define MUOS_FUNCTION_ASSIGN(target, expression)                                                                       \
    do {                                                                                                               \
        void *muos_function_symbol_ = (expression);                                                                    \
        _Static_assert(                                                                                                \
            sizeof(target) == sizeof(muos_function_symbol_), "incompatible function pointer representation"            \
        );                                                                                                             \
        memcpy(&(target), &muos_function_symbol_, sizeof(target));                                                     \
    } while (0)

#define MUOS_FUNCTION_EXPORT(destination, source)                                                                      \
    do {                                                                                                               \
        _Static_assert(sizeof(destination) == sizeof(source), "incompatible function pointer representation");         \
        memcpy(&(destination), &(source), sizeof(destination));                                                        \
    } while (0)
