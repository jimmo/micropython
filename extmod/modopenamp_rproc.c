/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2024 Arduino SA
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
 *
 * OpenAMP's remoteproc class.
 */
#if MICROPY_PY_OPENAMP_RPROC

#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "extmod/vfs.h"

#include "metal/alloc.h"
#include "metal/errno.h"
#include "metal/io.h"

#include "openamp/open_amp.h"
#include "openamp/remoteproc.h"
#include "openamp/remoteproc_loader.h"

#include "modopenamp.h"
#include "modopenamp_rproc.h"

#define DEBUG_printf(...)   // mp_printf(&mp_plat_print, __VA_ARGS__)

#if !MICROPY_PY_OPENAMP
#error "MICROPY_PY_OPENAMP_RPROC requires MICROPY_PY_OPENAMP"
#endif

typedef struct rproc_obj_t {
    mp_obj_base_t base;
    struct remoteproc rproc;
} rproc_obj_t;

const mp_obj_type_t rproc_type;

// Port-defined image store operations.
extern struct image_store_ops mp_openamp_rproc_store_ops;

// Port-defined remote-proc operations.
const struct remoteproc_ops mp_openamp_rproc_ops = {
    .init = mp_openamp_rproc_init,
    .mmap = mp_openamp_rproc_mmap,
    .start = mp_openamp_rproc_start,
    .stop = mp_openamp_rproc_stop,
    .config = mp_openamp_rproc_config,
    .remove = mp_openamp_rproc_remove,
    .shutdown = mp_openamp_rproc_shutdown,
};

STATIC mp_obj_t rproc_start(mp_obj_t self_in) {
    rproc_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // Start the processor to run the application.
    int error = remoteproc_start(&self->rproc);
    if (error != 0) {
        mp_raise_OSError(error);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(rproc_start_obj, rproc_start);

STATIC mp_obj_t rproc_stop(mp_obj_t self_in) {
    rproc_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // Stop the processor, but the processor is not powered down.
    int error = remoteproc_stop(&self->rproc);
    if (error != 0) {
        mp_raise_OSError(error);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(rproc_stop_obj, rproc_stop);

STATIC mp_obj_t rproc_shutdown(mp_obj_t self_in) {
    rproc_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // Shutdown the remoteproc and release its resources.
    int error = remoteproc_shutdown(&self->rproc);
    if (error != 0) {
        mp_raise_OSError(error);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(rproc_shutdown_obj, rproc_shutdown);

mp_obj_t rproc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_entry };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_entry, MP_ARG_OBJ | MP_ARG_REQUIRED,  {.u_rom_obj = MP_ROM_NONE } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rproc_obj_t *self = m_new_obj_with_finaliser(rproc_obj_t);
    self->base.type = &rproc_type;

    // Instantiate the remoteproc instance
    // NOTE: ports should use rproc->priv to allocate the image store,
    // which gets passed to remoteproc_load(), and all of the store ops.
    remoteproc_init(&self->rproc, &mp_openamp_rproc_ops, NULL);

    // Configure the remote before loading applications (optional).
    remoteproc_config(&self->rproc, NULL);

    if (mp_obj_is_int(args[ARG_entry].u_obj)) {
        self->rproc.bootaddr = mp_obj_get_int(args[ARG_entry].u_obj);
    } else {
        #if MICROPY_PY_OPENAMP_RPROC_ELFLD_ENABLE
        // Load firmware.
        const char *path = mp_obj_str_get_str(args[ARG_entry].u_obj);
        int error = remoteproc_load(&self->rproc, path, self->rproc.priv, &mp_openamp_rproc_store_ops, NULL);
        if (error != 0) {
            mp_raise_OSError(error);
        }
        #endif
    }
    return MP_OBJ_FROM_PTR(self);
}

STATIC const mp_rom_map_elem_t rproc_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_RProc) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&rproc_shutdown_obj) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&rproc_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&rproc_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_shutdown), MP_ROM_PTR(&rproc_shutdown_obj) },
};
STATIC MP_DEFINE_CONST_DICT(rproc_dict, rproc_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    rproc_type,
    MP_QSTR_RProc,
    MP_TYPE_FLAG_NONE,
    make_new, rproc_make_new,
    locals_dict, &rproc_dict
    );
#endif // MICROPY_PY_OPENAMP_RPROC
