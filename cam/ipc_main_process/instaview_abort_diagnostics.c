#ifdef IPC_INSTAVIEW_ABORT_DIAGNOSTICS

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef IPC_INSTAVIEW_SDK_BUILD_ID
#define IPC_INSTAVIEW_SDK_BUILD_ID "unknown"
#endif

typedef void (*abort_function_t)(void);
typedef void (*assert_function_t)(const char*, const char*, unsigned int, const char*);
typedef void (*stack_chk_fail_function_t)(void);

#define ABORT_DIAGNOSTIC_EXPORT __attribute__((visibility("default"), noreturn, no_stack_protector))

static volatile int _g_abort_diagnostic_logging;

static uintptr_t _abort_diagnostic_callsite(uintptr_t return_address)
{
    uintptr_t normalized_address = return_address & ~(uintptr_t)1U;
    if (!normalized_address) {
        return 0;
    }
    return normalized_address > 4U ? normalized_address - 4U : normalized_address;
}

static void _abort_diagnostic_write(const char* message, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(STDERR_FILENO, message + offset, length - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            break;
        }
        offset += (size_t)written;
    }
}

static void _abort_diagnostic_log(const char* reason, void* return_address)
{
    if (!__sync_bool_compare_and_swap(&_g_abort_diagnostic_logging, 0, 1)) {
        return;
    }

    uintptr_t return_value = (uintptr_t)return_address;
    uintptr_t callsite = _abort_diagnostic_callsite(return_value);
    Dl_info symbol_info;
    memset(&symbol_info, 0, sizeof(symbol_info));
    int symbol_found = callsite != 0 && dladdr((void*)callsite, &symbol_info) != 0;

    uintptr_t module_offset = 0;
    uintptr_t symbol_offset = 0;
    if (symbol_found && symbol_info.dli_fbase) {
        module_offset = callsite - (uintptr_t)symbol_info.dli_fbase;
    }
    if (symbol_found && symbol_info.dli_saddr) {
        symbol_offset = callsite - (uintptr_t)symbol_info.dli_saddr;
    }

    char message[768];
    int message_len = snprintf(
        message, sizeof(message),
        "[abort-diag] reason=%s tid=0x%lx return=%p callsite=%p "
        "module=%s module_offset=0x%lx symbol=%s symbol_offset=0x%lx "
        "instaview_sdk_build_id=%s\n",
        reason ? reason : "unknown", (unsigned long)pthread_self(), return_address,
        (void*)callsite,
        symbol_found && symbol_info.dli_fname ? symbol_info.dli_fname : "unknown",
        (unsigned long)module_offset,
        symbol_found && symbol_info.dli_sname ? symbol_info.dli_sname : "unknown",
        (unsigned long)symbol_offset, IPC_INSTAVIEW_SDK_BUILD_ID);
    if (message_len > 0) {
        size_t write_len = (size_t)message_len;
        if (write_len >= sizeof(message)) {
            write_len = sizeof(message) - 1;
        }
        _abort_diagnostic_write(message, write_len);
    }

    __sync_lock_release(&_g_abort_diagnostic_logging);
}

static int _abort_diagnostic_resolve(const char* name, void* function_storage)
{
    void* symbol = dlsym(RTLD_NEXT, name);
    memcpy(function_storage, &symbol, sizeof(symbol));
    return symbol != NULL;
}

ABORT_DIAGNOSTIC_EXPORT void abort(void)
{
    abort_function_t original_abort = NULL;
    void* return_address = __builtin_return_address(0);
    _abort_diagnostic_log("abort", return_address);
    if (_abort_diagnostic_resolve("abort", &original_abort)) {
        original_abort();
    }
    _exit(134);
}

ABORT_DIAGNOSTIC_EXPORT void __assert(const char* assertion, const char* file,
                                      unsigned int line, const char* function)
{
    assert_function_t original_assert = NULL;
    void* return_address = __builtin_return_address(0);
    _abort_diagnostic_log("__assert", return_address);
    if (_abort_diagnostic_resolve("__assert", &original_assert)) {
        original_assert(assertion, file, line, function);
    }
    _exit(134);
}

ABORT_DIAGNOSTIC_EXPORT void __stack_chk_fail(void)
{
    stack_chk_fail_function_t original_stack_chk_fail = NULL;
    void* return_address = __builtin_return_address(0);
    _abort_diagnostic_log("__stack_chk_fail", return_address);
    if (_abort_diagnostic_resolve("__stack_chk_fail", &original_stack_chk_fail)) {
        original_stack_chk_fail();
    }
    _exit(127);
}

#endif
