/* Sample third-party native module, written in C89. */
#include <stdio.h>
#include <string.h>
#include "../../../include/pipi_native.h"

static char greeting_buffer[1024];

static void copy_error(char *error, size_t cap, const char *message) {
    if (cap > 0U && error) {
        strncpy(error, message, cap - 1U);
        error[cap - 1U] = '\0';
    }
}

static int has_function(const char *name) {
    return strcmp(name, "clamp") == 0 ||
           strcmp(name, "greet") == 0 ||
           strcmp(name, "version") == 0 ||
           strcmp(name, "callCallback") == 0;
}

static int call_function(const PipiHostAPI *host, const char *name,
                         int argc, const PipiValue *argv, PipiValue *result,
                         char *error, size_t error_size) {
    if (strcmp(name, "clamp") == 0) {
        double value, low, high;
        if (argc != 3 || argv[0].type != PIPI_VALUE_NUMBER ||
            argv[1].type != PIPI_VALUE_NUMBER || argv[2].type != PIPI_VALUE_NUMBER) {
            copy_error(error, error_size, "clamp(value, low, high) expects three numbers");
            return 0;
        }
        value = argv[0].number;
        low = argv[1].number;
        high = argv[2].number;
        if (low > high) {
            copy_error(error, error_size, "clamp low bound must be <= high bound");
            return 0;
        }
        if (value < low) value = low;
        if (value > high) value = high;
        result->type = PIPI_VALUE_NUMBER;
        result->number = value;
        result->string = "";
        result->boolean = 0;
        return 1;
    }
    if (strcmp(name, "greet") == 0) {
        const char *who;
        if (argc != 1) {
            copy_error(error, error_size, "greet(name) expects one argument");
            return 0;
        }
        who = argv[0].string ? argv[0].string : "bestie";
        sprintf(greeting_buffer, "PIPI says hi to %.980s", who);
        result->type = PIPI_VALUE_STRING;
        result->number = 0.0;
        result->string = greeting_buffer;
        result->boolean = 0;
        return 1;
    }
    if (strcmp(name, "version") == 0) {
        result->type = PIPI_VALUE_STRING;
        result->number = 0.0;
        result->string = "pipi/extra 1.0.0 (native ABI v1)";
        result->boolean = 0;
        return 1;
    }
    if (strcmp(name, "callCallback") == 0) {
        PipiValue callback_result;
        if (argc != 2 || argv[0].type != PIPI_VALUE_FUNCTION) {
            copy_error(error, error_size, "callCallback(function, value) expects a PIPI function and a value");
            return 0;
        }
        if (!host || !host->call_function) {
            copy_error(error, error_size, "host callback API unavailable");
            return 0;
        }
        if (!host->call_function(host->userdata, argv[0].string, 1, &argv[1], &callback_result)) {
            copy_error(error, error_size, "PIPI callback failed");
            return 0;
        }
        *result = callback_result;
        return 1;
    }
    copy_error(error, error_size, "unknown function in pipi/extra module");
    return 0;
}

static int initialize(const PipiHostAPI *host, char *error, size_t error_size) {
    (void)error;
    (void)error_size;
    if (!host || host->abi_version != PIPI_NATIVE_ABI_VERSION) return 0;
    return 1;
}

static void shutdown_module(const PipiHostAPI *host) { (void)host; }

static const PipiNativeModuleV1 module_api = {
    PIPI_NATIVE_ABI_VERSION,
    "pipi/extra",
    "1.0.0",
    initialize,
    has_function,
    call_function,
    shutdown_module
};

PIPI_NATIVE_EXPORT const PipiNativeModuleV1 *pipi_native_module_v1(void) {
    return &module_api;
}
