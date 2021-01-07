/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Options to control how MicroPython is built for this port,
// overriding defaults in py/mpconfig.h.

// For size_t and ssize_t
#include <unistd.h>

// Variant-specific definitions.
#include "mpconfigvariant.h"

// Enable the minimum possible features to allow the Unix port to function.
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_HELPER_LEXER_UNIX   (1)
#define MICROPY_HELPER_REPL         (1)
#define MICROPY_KBD_EXCEPTION       (1)
#define MICROPY_READER_POSIX        (1)

#define MICROPY_PY_SYS              (1)

#define MICROPY_ALLOC_PATH_MAX      (PATH_MAX)

#ifndef MICROPY_PY_IO
#define MICROPY_PY_IO               (0)
#endif

#define MP_STATE_PORT MP_STATE_VM

// Fall back to setjmp() implementation for discovery of GC pointers in registers.
#if !(defined(MICROPY_GCREGS_SETJMP) || defined(__x86_64__) || defined(__i386__) || defined(__thumb2__) || defined(__thumb__) || defined(__arm__))
#define MICROPY_GCREGS_SETJMP (1)
#endif

// Provide a default sys.platform if the variant doesn't provide one.
#ifndef MICROPY_PY_SYS_PLATFORM
#if defined(__APPLE__) && defined(__MACH__)
    #define MICROPY_PY_SYS_PLATFORM  "darwin"
#else
    #define MICROPY_PY_SYS_PLATFORM  "linux"
#endif
#endif

// assume that if we already defined the obj repr then we also defined types
#ifndef MICROPY_OBJ_REPR
#ifdef __LP64__
typedef long mp_int_t; // must be pointer size
typedef unsigned long mp_uint_t; // must be pointer size
#else
// These are definitions for machines where sizeof(int) == sizeof(void*),
// regardless of actual size.
typedef int mp_int_t; // must be pointer size
typedef unsigned int mp_uint_t; // must be pointer size
#endif
#endif

// Cannot include <sys/types.h>, as it may lead to symbol name clashes
#if _FILE_OFFSET_BITS == 64 && !defined(__LP64__)
typedef long long mp_off_t;
#else
typedef long mp_off_t;
#endif

// We need to provide a declaration/definition of alloca()
// unless support for it is disabled.
#if !defined(MICROPY_NO_ALLOCA) || MICROPY_NO_ALLOCA == 0
#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <alloca.h>
#endif
#endif

// From "man readdir": "Under glibc, programs can check for the availability
// of the fields [in struct dirent] not defined in POSIX.1 by testing whether
// the macros [...], _DIRENT_HAVE_D_TYPE are defined."
// Other libc's don't define it, but proactively assume that dirent->d_type
// is available on a modern *nix system.
#ifndef _DIRENT_HAVE_D_TYPE
#define _DIRENT_HAVE_D_TYPE (1)
#endif

// This macro is not provided by glibc but we need it so ports that don't have
// dirent->d_ino can disable the use of this field.
#ifndef _DIRENT_HAVE_D_INO
#define _DIRENT_HAVE_D_INO (1)
#endif

#ifndef __APPLE__
// For debugging purposes, make printf() available to any source file.
#include <stdio.h>
#endif

#ifdef __ANDROID__
#include <android/api-level.h>
#if __ANDROID_API__ < 4
// Bionic libc in Android 1.5 misses these 2 functions
#define MP_NEED_LOG2 (1)
#define nan(x) NAN
#endif
#endif

#ifdef __linux__
// Can access physical memory using /dev/mem
#define MICROPY_PLAT_DEV_MEM  (1)
#endif

// Provided in alloc.c, necessary for the native emitter (and FFI).
void mp_unix_alloc_exec(size_t min_size, void **ptr, size_t *size);
void mp_unix_free_exec(void *ptr, size_t size);
void mp_unix_mark_exec(void);
#define MP_PLAT_ALLOC_EXEC(min_size, ptr, size) mp_unix_alloc_exec(min_size, ptr, size)
#define MP_PLAT_FREE_EXEC(ptr, size) mp_unix_free_exec(ptr, size)
#ifndef MICROPY_FORCE_PLAT_ALLOC_EXEC
// Use MP_PLAT_ALLOC_EXEC for any executable memory allocation, including for FFI
// (overriding libffi own implementation)
#define MICROPY_FORCE_PLAT_ALLOC_EXEC (1)
#endif

// Assume that select() call, interrupted with a signal, and erroring
// with EINTR, updates remaining timeout value.
#define MICROPY_SELECT_REMAINING_TIME (1)

#if MICROPY_PY_THREAD
#define MICROPY_BEGIN_ATOMIC_SECTION() (mp_thread_unix_begin_atomic_section(), 0xffffffff)
#define MICROPY_END_ATOMIC_SECTION(x) (void)x; mp_thread_unix_end_atomic_section()
#endif

#define MICROPY_EVENT_POLL_HOOK \
    do { \
        extern void mp_handle_pending(bool); \
        mp_handle_pending(true); \
        mp_hal_delay_us(500); \
    } while (0);

#include <sched.h>
#define MICROPY_UNIX_MACHINE_IDLE sched_yield();


extern const struct _mp_obj_module_t mp_module_machine;
extern const struct _mp_obj_module_t mp_module_os;
extern const struct _mp_obj_module_t mp_module_uos_vfs;
extern const struct _mp_obj_module_t mp_module_time;
extern const struct _mp_obj_module_t mp_module_termios;

#if MICROPY_PY_MACHINE
#define MICROPY_PY_MACHINE_DEF { MP_ROM_QSTR(MP_QSTR_umachine), MP_ROM_PTR(&mp_module_machine) },
#else
#define MICROPY_PY_MACHINE_DEF
#endif
#if MICROPY_PY_UOS_VFS
#define MICROPY_PY_UOS_DEF { MP_ROM_QSTR(MP_QSTR_uos), MP_ROM_PTR(&mp_module_uos_vfs) },
#else
#define MICROPY_PY_UOS_DEF { MP_ROM_QSTR(MP_QSTR_uos), MP_ROM_PTR(&mp_module_os) },
#endif
#if MICROPY_PY_USELECT_POSIX
#define MICROPY_PY_USELECT_DEF { MP_ROM_QSTR(MP_QSTR_uselect), MP_ROM_PTR(&mp_module_uselect) },
#else
#define MICROPY_PY_USELECT_DEF
#endif
#if MICROPY_PY_USOCKET
#define MICROPY_PY_USOCKET_DEF { MP_ROM_QSTR(MP_QSTR_usocket), MP_ROM_PTR(&mp_module_socket) },
#else
#define MICROPY_PY_USOCKET_DEF
#endif
#if MICROPY_PY_UTIME
#define MICROPY_PY_UTIME_DEF { MP_ROM_QSTR(MP_QSTR_utime), MP_ROM_PTR(&mp_module_time) },
#else
#define MICROPY_PY_UTIME_DEF
#endif

#ifndef MICROPY_VARIANT_BUILTIN_MODULES
#define MICROPY_VARIANT_BUILTIN_MODULES
#endif

#define MICROPY_PORT_BUILTIN_MODULES \
    MICROPY_PY_MACHINE_DEF \
    MICROPY_PY_UOS_DEF \
    MICROPY_PY_USELECT_DEF \
    MICROPY_PY_USOCKET_DEF \
    MICROPY_PY_UTIME_DEF \
    MICROPY_VARIANT_BUILTIN_MODULES

#ifndef MICROPY_VARIANT_ROOT_POINTERS
#define MICROPY_VARIANT_ROOT_POINTERS
#endif

#define MICROPY_PORT_ROOT_POINTERS \
    MICROPY_VARIANT_ROOT_POINTERS
