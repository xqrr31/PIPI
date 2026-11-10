#ifndef PIPI_NATIVE_H
#define PIPI_NATIVE_H
/* PIPI Native Module ABI v1. Keep this header C89-compatible. */
#include <stddef.h>

#define PIPI_NATIVE_ABI_VERSION 1UL

#if defined(_WIN32)
# if defined(PIPI_NATIVE_BUILD)
#  define PIPI_NATIVE_EXPORT __declspec(dllexport)
# else
#  define PIPI_NATIVE_EXPORT
# endif
#else
# define PIPI_NATIVE_EXPORT __attribute__((visibility("default")))
#endif

enum {
    PIPI_VALUE_NULL = 0,
    PIPI_VALUE_NUMBER = 1,
    PIPI_VALUE_STRING = 2,
    PIPI_VALUE_BOOLEAN = 3,
    PIPI_VALUE_FUNCTION = 4
};

typedef struct PipiValue {
    int type;
    double number;
    const char *string;
    int boolean;
} PipiValue;

struct PipiHostAPI;
typedef struct PipiHostAPI PipiHostAPI;

struct PipiHostAPI {
    unsigned long abi_version;
    void *userdata;
    void (*log)(void *userdata, int level, const char *message);
    void (*set_error)(void *userdata, const char *message);
    int (*call_function)(void *userdata, const char *function_name,
                         int argc, const PipiValue *argv, PipiValue *result);
    const char *(*get_env)(void *userdata, const char *name);
};

typedef struct PipiNativeModuleV1 {
    unsigned long abi_version;
    const char *module_name;
    const char *module_version;
    int (*initialize)(const PipiHostAPI *host, char *error, size_t error_size);
    int (*has_function)(const char *name);
    int (*call)(const PipiHostAPI *host, const char *name,
                int argc, const PipiValue *argv, PipiValue *result,
                char *error, size_t error_size);
    void (*shutdown)(const PipiHostAPI *host);
} PipiNativeModuleV1;

typedef const PipiNativeModuleV1 *(*PipiNativeModuleEntryV1)(void);

/* Export exactly this entry point from every native module. */
#ifdef __cplusplus
extern "C" {
#endif
PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void);
#ifdef __cplusplus
}
#endif

/* ABI contract:
 * - argv values and strings are borrowed for the duration of call().
 * - result strings may point to static/module-owned memory; the VM copies a
 *   returned string before call() returns to PIPI code.
 * - return nonzero from call() on success; on failure put a message in error.
 * - the descriptor and function table must remain valid until shutdown().
 * - native plugins execute with process privileges; load only trusted modules.
 */
#endif
