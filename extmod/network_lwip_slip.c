/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2019 Damien P. George
 * Copyright (c) 2015 Galen Hazelwood
 * Copyright (c) 2015-2017 Paul Sokolovsky
 * Copyright (c) 2022 Jim Mussared
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

#include "py/runtime.h"

#if MICROPY_PY_NETWORK && MICROPY_PY_LWIP && MICROPY_PY_LWIP_SLIP

#include "netif/slipif.h"
#include "lwip/sio.h"

/******************************************************************************/
// Slip object for modlwip. Requires a serial driver for the port that supports
// the lwip serial callback functions.

typedef struct _lwip_slip_obj_t {
    mp_obj_base_t base;
    struct netif lwip_netif;
} lwip_slip_obj_t;

// Slip object is unique for now. Possibly can fix this later. FIXME
STATIC lwip_slip_obj_t lwip_slip_obj;

// Declare these early.
void mod_lwip_register_poll(void (*poll)(void *arg), void *poll_arg);
void mod_lwip_deregister_poll(void (*poll)(void *arg), void *poll_arg);

STATIC void slip_lwip_poll(void *netif) {
    slipif_poll((struct netif *)netif);
}

STATIC const mp_obj_type_t lwip_slip_type;

// lwIP SLIP callback functions
sio_fd_t sio_open(u8_t dvnum) {
    // We support singleton SLIP interface, so just return any truish value.
    return (sio_fd_t)1;
}

void sio_send(u8_t c, sio_fd_t fd) {
    mp_obj_type_t *type = mp_obj_get_type(MP_STATE_VM(lwip_slip_stream));
    int error;
    type->stream_p->write(MP_STATE_VM(lwip_slip_stream), &c, 1, &error);
}

u32_t sio_tryread(sio_fd_t fd, u8_t *data, u32_t len) {
    mp_obj_type_t *type = mp_obj_get_type(MP_STATE_VM(lwip_slip_stream));
    int error;
    mp_uint_t out_sz = type->stream_p->read(MP_STATE_VM(lwip_slip_stream), data, len, &error);
    if (out_sz == MP_STREAM_ERROR) {
        if (mp_is_nonblocking_error(error)) {
            return 0;
        }
        // Can't do much else, can we?
        return 0;
    }
    return out_sz;
}

// constructor lwip.slip(device=integer, iplocal=string, ipremote=string)
STATIC mp_obj_t lwip_slip_make_new(mp_obj_t type_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 3, 3, false);

    lwip_slip_obj.base.type = &lwip_slip_type;

    MP_STATE_VM(lwip_slip_stream) = args[0];

    ip_addr_t iplocal, ipremote;
    if (!ipaddr_aton(mp_obj_str_get_str(args[1]), &iplocal)) {
        mp_raise_ValueError(MP_ERROR_TEXT("not a valid local IP"));
    }
    if (!ipaddr_aton(mp_obj_str_get_str(args[2]), &ipremote)) {
        mp_raise_ValueError(MP_ERROR_TEXT("not a valid remote IP"));
    }

    struct netif *n = &lwip_slip_obj.lwip_netif;
    if (netif_add(n, &iplocal, IP_ADDR_BROADCAST, &ipremote, NULL, slipif_init, ip_input) == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("out of memory"));
    }
    netif_set_up(n);
    netif_set_default(n);
    mod_lwip_register_poll(slip_lwip_poll, n);

    return (mp_obj_t)&lwip_slip_obj;
}

STATIC mp_obj_t lwip_slip_status(mp_obj_t self_in) {
    // Null function for now.
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(lwip_slip_status_obj, lwip_slip_status);

STATIC const mp_rom_map_elem_t lwip_slip_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&lwip_slip_status_obj) },
};

STATIC MP_DEFINE_CONST_DICT(lwip_slip_locals_dict, lwip_slip_locals_dict_table);

STATIC MP_DEFINE_CONST_OBJ_TYPE(
    lwip_slip_type,
    MP_QSTR_slip,
    MP_TYPE_FLAG_NONE,
    make_new, lwip_slip_make_new,
    locals_dict, &lwip_slip_locals_dict
    );

MP_REGISTER_ROOT_POINTER(mp_obj_t lwip_slip_stream);

#endif // MICROPY_PY_LWIP_SLIP
