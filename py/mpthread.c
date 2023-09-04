/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Damien George
 * Copyright (c) 2023 Jim Mussared
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

#include <stdarg.h>

#include "py/mpthread.h"
#include "py/stackctrl.h"
#include "py/runtime.h"

#if MICROPY_PY_THREAD && MICROPY_PY_THREAD_RTOS

#if MICROPY_PY_THREAD_GIL

#if MICROPY_ENABLE_PYSTACK
#error not supported
#endif

// On some systems the BLE event callbacks may occur on a system thread which is not
// a MicroPython thread.  In such cases the callback must set up relevant MicroPython
// state and obtain the GIL, to synchronised with the rest of the runtime.

void mp_thread_run_on_mp_thread(mp_run_on_thread_function_t fn, void *arg) {
    // This code may run on an existing MicroPython thread, or a non-MicroPython thread
    // that's not using the mp_thread_get_state() value.  In the former case the state
    // must be restored once this callback finishes.
    mp_state_thread_t *ts_orig = mp_thread_get_state();

    mp_state_thread_t ts;
    if (ts_orig == NULL) {
        mp_thread_set_state(&ts);
        mp_stack_set_top(&ts + 1); // need to include ts in root-pointer scan
        mp_stack_set_limit(4096);// TODO MICROPY_PY_BLUETOOTH_SYNC_EVENT_STACK_SIZE - 1024);
        ts.gc_lock_depth = 0;
        ts.mp_pending_exception = MP_OBJ_NULL;
        mp_locals_set(mp_state_ctx.thread.dict_locals); // set from the outer context
        mp_globals_set(mp_state_ctx.thread.dict_globals); // set from the outer context
        MP_THREAD_GIL_ENTER();
    }

    mp_sched_lock();
    fn(arg);
    mp_sched_unlock();

    if (ts_orig == NULL) {
        MP_THREAD_GIL_EXIT();
        mp_thread_set_state(ts_orig);
    }
}

#else // !MICROPY_PY_THREAD_GIL

void mp_thread_run_on_mp_thread(mp_run_on_thread_function_t fn, void *arg) {
    #error "Not supported"
}

#endif // MICROPY_PY_THREAD_GIL

#endif // MICROPY_PY_THREAD && MICROPY_PY_THREAD_RTOS
