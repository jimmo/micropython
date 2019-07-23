/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ayke van Laethem
 * Copyright (c) 2019 Jim Mussared
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

#include "py/binary.h"
#include "py/misc.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/objarray.h"
#include "py/qstr.h"
#include "py/ringbuf.h"
#include "py/runtime.h"
#include "extmod/modbluetooth.h"
#include <string.h>

#if MICROPY_PY_BLUETOOTH

#if !MP_BT_CALLBACK_ALLOC && !MICROPY_ENABLE_SCHEDULER
#error MICROPY_PY_BLUETOOTH requires MICROPY_ENABLE_SCHEDULER when MP_BT_CALLBACK_ALLOC==0
#endif

// Event codes for the IRQ handler.
// Can also be combined to pass to the trigger param to select which events you are interested in.
// Note this is currently stored in a uint16_t (trigger, event), so one bit free.
#define MP_BT_IRQ_CENTRAL_CONNECT                  (1 << 1)
#define MP_BT_IRQ_CENTRAL_DISCONNECT               (1 << 2)
#define MP_BT_IRQ_CHR_WRITE                        (1 << 3)
#define MP_BT_IRQ_SCAN_RESULT                      (1 << 4)
#define MP_BT_IRQ_SCAN_COMPLETE                    (1 << 5)
#define MP_BT_IRQ_PERIPHERAL_CONNECT               (1 << 6)
#define MP_BT_IRQ_PERIPHERAL_DISCONNECT            (1 << 7)
#define MP_BT_IRQ_PERIPHERAL_SVC_RESULT            (1 << 8)
#define MP_BT_IRQ_PERIPHERAL_CHR_RESULT            (1 << 9)
#define MP_BT_IRQ_PERIPHERAL_DSC_RESULT            (1 << 10)
#define MP_BT_IRQ_PERIPHERAL_READ_RESULT           (1 << 11)
#define MP_BT_IRQ_PERIPHERAL_WRITE_STATUS          (1 << 12)
#define MP_BT_IRQ_PERIPHERAL_NOTIFY                (1 << 13)
#define MP_BT_IRQ_PERIPHERAL_INDICATE              (1 << 14)
#define MP_BT_IRQ_ALL                              (0xffff)

/*
from micropython import const
IRQ_CENTRAL_CONNECT                  = const(1 << 1)
IRQ_CENTRAL_DISCONNECT               = const(1 << 2)
IRQ_CHR_WRITE                        = const(1 << 3)
IRQ_SCAN_RESULT                      = const(1 << 4)
IRQ_SCAN_COMPLETE                    = const(1 << 5)
IRQ_PERIPHERAL_CONNECT               = const(1 << 6)
IRQ_PERIPHERAL_DISCONNECT            = const(1 << 7)
IRQ_PERIPHERAL_SVC_RESULT            = const(1 << 8)
IRQ_PERIPHERAL_CHR_RESULT            = const(1 << 9)
IRQ_PERIPHERAL_DSC_RESULT            = const(1 << 10)
IRQ_PERIPHERAL_READ_RESULT           = const(1 << 11)
IRQ_PERIPHERAL_WRITE_STATUS          = const(1 << 12)
IRQ_PERIPHERAL_NOTIFY                = const(1 << 13)
IRQ_PERIPHERAL_INDICATE              = const(1 << 14)
IRQ_ALL                              = const(0xffff)
*/

#define MP_BT_CONNECT_DEFAULT_SCAN_DURATION_MS 2000

STATIC const mp_obj_type_t bluetooth_type;
STATIC const mp_obj_type_t uuid_type;

typedef struct {
    mp_obj_base_t base;
    mp_obj_t irq_handler;
    uint16_t irq_trigger;
    ringbuf_t ringbuf;
} mp_obj_bluetooth_t;

// TODO: this seems like it could be generic?
STATIC mp_obj_t bluetooth_handle_errno(int err) {
    if (err != 0) {
        mp_raise_OSError(err);
    }
    return mp_const_none;
}

// ----------------------------------------------------------------------------
// UUID object
// ----------------------------------------------------------------------------

// Parse string UUIDs, which are expected to be 128-bit UUIDs.
STATIC void mp_bt_parse_uuid_128bit_str(mp_obj_t obj, uint8_t *uuid) {
    GET_STR_DATA_LEN(obj, str_data, str_len);
    int uuid_i = 32;
    for (int i = 0; i < str_len; i++) {
        char c = str_data[i];
        if (c == '-') {
            continue;
        }
        if (c >= '0' && c <= '9') {
            c = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            c = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            c = c - 'A' + 10;
        } else {
            mp_raise_ValueError("unknown char in UUID");
        }
        uuid_i--;
        if (uuid_i < 0) {
            mp_raise_ValueError("UUID too long");
        }
        if (uuid_i % 2 == 0) {
            // lower nibble
            uuid[uuid_i/2] |= c;
        } else {
            // upper nibble
            uuid[uuid_i/2] = c << 4;
        }
    }
    if (uuid_i > 0) {
        mp_raise_ValueError("UUID too short");
    }
}

STATIC mp_obj_t uuid_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    mp_obj_bt_uuid_t *self = m_new_obj(mp_obj_bt_uuid_t);
    self->base.type = &uuid_type;

    if (mp_obj_is_int(all_args[0])) {
        self->type = MP_BT_UUID_TYPE_16;
        mp_int_t value = mp_obj_get_int(all_args[0]);
        if (value > 65535) {
            mp_raise_ValueError("invalid UUID");
        }
        self->uuid16 = value;
    } else if (mp_obj_is_str(all_args[0])) {
        self->type = MP_BT_UUID_TYPE_128;
        mp_bt_parse_uuid_128bit_str(all_args[0], self->uuid128);
    } // TODO: else if bytes.

    return self;
}

STATIC void uuid_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_obj_bt_uuid_t *self = MP_OBJ_TO_PTR(self_in);

    if (self->type == MP_BT_UUID_TYPE_16) {
        mp_printf(print, "UUID16(0x%04x)", self->uuid16);
    } else if (self->type == MP_BT_UUID_TYPE_32) {
        mp_printf(print, "UUID32(0x%08x)", self->uuid32);
    } else if (self->type == MP_BT_UUID_TYPE_128) {
        mp_printf(print, "UUID128('");
        for (int i = 0; i < 16; ++i) {
            mp_printf(print, "%02x", self->uuid128[15-i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) {
                mp_printf(print, "-");
            }
        }
        mp_printf(print, "')");
    } else {
        mp_printf(print, "UUID?(%d)", self->type);
    }
}

STATIC void ringbuf_put_uuid(ringbuf_t *ringbuf, mp_obj_bt_uuid_t *uuid) {
    assert(ringbuf_free(ringbuf) >= uuid->type + 1);
    ringbuf_put(ringbuf, uuid->type);
    switch (uuid->type) {
        case MP_BT_UUID_TYPE_16:
            ringbuf_put16(ringbuf, uuid->uuid16);
            break;
        case MP_BT_UUID_TYPE_32:
            ringbuf_put16(ringbuf, uuid->uuid32 >> 16);
            ringbuf_put16(ringbuf, uuid->uuid32 & 0xffff);
            break;
        case MP_BT_UUID_TYPE_128:
            for (int i = 0; i < 16; ++i) {
                ringbuf_put(ringbuf, uuid->uuid128[i]);
            }
            break;
    }
}

STATIC mp_obj_bt_uuid_t* ringbuf_get_uuid(ringbuf_t *ringbuf) {
    mp_obj_bt_uuid_t *uuid = m_new_obj(mp_obj_bt_uuid_t);
    uuid->base.type = &uuid_type;
    assert(ringbuf_avail(ringbuf) >= 1);
    uuid->type = ringbuf_get(ringbuf);
    assert(ringbuf_avail(ringbuf) >= uuid->type);
    uint16_t h, l;
    switch (uuid->type) {
        case MP_BT_UUID_TYPE_16:
            uuid->uuid16 = ringbuf_get16(ringbuf);
            break;
        case MP_BT_UUID_TYPE_32:
            h = ringbuf_get16(ringbuf);
            l = ringbuf_get16(ringbuf);
            uuid->uuid32 = (h << 16) | l;
            break;
        case MP_BT_UUID_TYPE_128:
            for (int i = 0; i < 16; ++i) {
                uuid->uuid128[i] = ringbuf_get(ringbuf);
            }
            break;
    }
    return uuid;
}

STATIC const mp_rom_map_elem_t uuid_locals_dict_table[] = {
};
STATIC MP_DEFINE_CONST_DICT(uuid_locals_dict, uuid_locals_dict_table);

STATIC const mp_obj_type_t uuid_type = {
    { &mp_type_type },
    .name = MP_QSTR_UUID,
    .make_new = uuid_make_new,
    .locals_dict = (void*)&uuid_locals_dict,
    .print = uuid_print,
};

// ----------------------------------------------------------------------------
// Bluetooth object: General
// ----------------------------------------------------------------------------

STATIC mp_obj_t bluetooth_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    if (MP_STATE_VM(bluetooth) == MP_OBJ_NULL) {
        mp_obj_bluetooth_t *o = m_new_obj(mp_obj_bluetooth_t);
        o->base.type = &bluetooth_type;
        o->irq_handler = mp_const_none;
        o->irq_trigger = 0;
        ringbuf_alloc(&o->ringbuf, MP_BT_RINGBUF_SIZE);
        MP_STATE_VM(bluetooth) = MP_OBJ_FROM_PTR(o);
    }
    mp_obj_t result = MP_STATE_VM(bluetooth);
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    return result;
}

STATIC mp_obj_t bluetooth_active(size_t n_args, const mp_obj_t *args) {
    // TODO: Should active(False) clear the IRQ?
    //self->irq_handler = mp_const_none;
    //self->irq_trigger = 0;

    if (n_args == 2) { // boolean enable/disable argument supplied
        if (mp_obj_is_true(args[1])) {
            int err = mp_bt_enable();
            if (err != 0) {
                mp_raise_OSError(err);
            }
        } else {
            mp_bt_disable();
        }
    }
    return mp_obj_new_bool(mp_bt_is_enabled());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bluetooth_active_obj, 1, 2, bluetooth_active);

STATIC mp_obj_t bluetooth_config(mp_obj_t self_in, mp_obj_t param) {
    if (param == MP_OBJ_NEW_QSTR(MP_QSTR_mac)) {
        uint8_t addr[6];
        mp_bt_get_addr(addr);
        return mp_obj_new_bytes(addr, MP_ARRAY_SIZE(addr));
    } else {
        mp_raise_ValueError("unknown config param");
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bluetooth_config_obj, bluetooth_config);

// TODO: consider making trigger optional if handler=None
STATIC mp_obj_t bluetooth_irq(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_handler, ARG_trigger };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_handler, MP_ARG_OBJ|MP_ARG_REQUIRED, {.u_obj = mp_const_none} },
        { MP_QSTR_trigger, MP_ARG_INT|MP_ARG_REQUIRED, {.u_int = 0} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    mp_obj_t callback = args[ARG_handler].u_obj;
    if (callback != mp_const_none && !mp_obj_is_fun(callback)) {
        mp_raise_ValueError("invalid callback");
    }

    // Update the callback.
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t* bt = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bt->irq_handler = callback;
    bt->irq_trigger = args[ARG_trigger].u_int;
    MICROPY_END_ATOMIC_SECTION(atomic_state);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bluetooth_irq_obj, 1, bluetooth_irq);

// ----------------------------------------------------------------------------
// Bluetooth object: GAP
// ----------------------------------------------------------------------------

STATIC mp_obj_t bluetooth_advertise(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_interval_ms, ARG_adv_data, ARG_resp_data, ARG_connectable };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_interval_ms, MP_ARG_INT, {.u_int = 100} },
        { MP_QSTR_adv_data, MP_ARG_OBJ, {.u_obj = mp_const_none } },
        { MP_QSTR_resp_data, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = mp_const_none } },
        { MP_QSTR_connectable, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = mp_const_true } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t interval_ms = args[ARG_interval_ms].u_int;

    // TODO: Should we allow no adv_data, and just use interval_ms == 0 as the stop condition?
    if (interval_ms == 0 || args[ARG_adv_data].u_obj == mp_const_none) {
        mp_bt_advertise_stop();
    }

    mp_bt_adv_type_t adv_type = MP_BT_ADV_TYPE_ADV_IND; // connectable=True
    if (!mp_obj_is_true(args[ARG_connectable].u_obj)) {
        adv_type = MP_BT_ADV_TYPE_ADV_NONCONN_IND; // connectable=False
    }

    mp_buffer_info_t adv_bufinfo = {0};
    mp_get_buffer_raise(args[ARG_adv_data].u_obj, &adv_bufinfo, MP_BUFFER_READ);

    mp_buffer_info_t resp_bufinfo = {0};
    if (args[ARG_resp_data].u_obj != mp_const_none) {
        mp_get_buffer_raise(args[ARG_resp_data].u_obj, &resp_bufinfo, MP_BUFFER_READ);
    }

    return bluetooth_handle_errno(mp_bt_advertise_start(adv_type, interval_ms, adv_bufinfo.buf, adv_bufinfo.len, resp_bufinfo.buf, resp_bufinfo.len));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bluetooth_advertise_obj, 1, bluetooth_advertise);

STATIC mp_obj_t bluetooth_gatts_add_svc(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_uuid, ARG_chrs };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid, MP_ARG_OBJ | MP_ARG_REQUIRED },
        { MP_QSTR_chrs, MP_ARG_OBJ | MP_ARG_REQUIRED },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (!mp_obj_is_type(args[ARG_uuid].u_obj, &uuid_type)) {
        mp_raise_ValueError("invalid UUID");
    }
    mp_obj_bt_uuid_t *svc_uuid = MP_OBJ_TO_PTR(args[ARG_uuid].u_obj);

    // TODO: Maybe make this work with any subscriptable type (not just tuple).
    if (!mp_obj_is_type(args[ARG_chrs].u_obj, &mp_type_tuple)) {
        mp_raise_ValueError("invalid chrs tuple");
    }
    mp_obj_tuple_t *chrs = MP_OBJ_TO_PTR(args[ARG_chrs].u_obj);

    mp_obj_bt_uuid_t **chr_uuids = m_new(mp_obj_bt_uuid_t*, chrs->len);
    uint8_t *chr_flags = m_new(uint8_t, chrs->len);
    uint16_t *value_handles = m_new(uint16_t, chrs->len);

    // Extract out characteristic uuids & flags.
    for (int i = 0; i < chrs->len; i++) {
        mp_obj_t chr_obj = chrs->items[i];
        mp_obj_tuple_t *chr = MP_OBJ_TO_PTR(chr_obj);

        if (!mp_obj_is_type(chr_obj, &mp_type_tuple) || chr->len != 2) {
            mp_raise_ValueError("invalid chr tuple");
        }
        mp_obj_t uuid_obj = chr->items[0];
        if (!mp_obj_is_type(uuid_obj, &uuid_type)) {
            mp_raise_ValueError("invalid chr uuid");
        }
        chr_uuids[i] = MP_OBJ_TO_PTR(uuid_obj);
        chr_flags[i] = mp_obj_get_int(chr->items[1]);
        value_handles[i] = MP_BT_INVALID_VALUE_HANDLE;
    }

    // Add service.
    int err = mp_bt_add_svc(svc_uuid, chr_uuids, chr_flags, value_handles, chrs->len);
    bluetooth_handle_errno(err);

    // Return tuple of value handles.
    mp_obj_tuple_t *result = mp_obj_new_tuple(chrs->len, NULL);
    for (int i = 0; i < chrs->len; i++) {
        result->items[i] = MP_OBJ_NEW_SMALL_INT(value_handles[i]);
    }
    return MP_OBJ_FROM_PTR(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bluetooth_gatts_add_svc_obj, 1, bluetooth_gatts_add_svc);

#if MP_BT_ENABLE_CENTRAL_MODE
STATIC mp_obj_t bluetooth_connect(size_t n_args, const mp_obj_t *args) {
    uint8_t addr_type = mp_obj_get_int(args[1]);
    mp_buffer_info_t bufinfo = {0};
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len != 6) {
        mp_raise_ValueError("invalid addr");
    }
    mp_int_t scan_duration_ms = MP_BT_CONNECT_DEFAULT_SCAN_DURATION_MS;
    if (n_args == 4) {
        mp_obj_get_int_maybe(args[3], &scan_duration_ms);
    }

    int err = mp_bt_peripheral_connect(addr_type, bufinfo.buf, scan_duration_ms);
    return bluetooth_handle_errno(err);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bluetooth_connect_obj, 3, 4, bluetooth_connect);

STATIC mp_obj_t bluetooth_scan(size_t n_args, const mp_obj_t *args) {
    if (n_args == 2 && args[1] == mp_const_none) {
        int err = mp_bt_scan_stop();
        return bluetooth_handle_errno(err);
    } else {
        mp_int_t duration_ms = 0;
        if (n_args == 2) {
            if (!mp_obj_is_int(args[1])) {
                mp_raise_ValueError("invalid duration");
            }
            mp_obj_get_int_maybe(args[1], &duration_ms);
        }

        int err = mp_bt_scan_start(duration_ms);
        return bluetooth_handle_errno(err);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bluetooth_scan_obj, 1, 2, bluetooth_scan);
#endif // MP_BT_ENABLE_CENTRAL_MODE

STATIC mp_obj_t bluetooth_disconnect(mp_obj_t self_in, mp_obj_t conn_handle_in) {
    uint16_t conn_handle = mp_obj_get_int(conn_handle_in);
    int err = mp_bt_disconnect(conn_handle);
    return bluetooth_handle_errno(err);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bluetooth_disconnect_obj, bluetooth_disconnect);

// ----------------------------------------------------------------------------
// Bluetooth object: GATTS (Peripheral/Advertiser role)
// ----------------------------------------------------------------------------

STATIC mp_obj_t bluetooth_gatts_read(mp_obj_t self_in, mp_obj_t value_handle_in) {
    uint8_t buf[MP_BT_MAX_ATTR_SIZE];
    size_t len = sizeof(buf);
    mp_bt_chr_value_read(mp_obj_get_int(value_handle_in), buf, &len);
    return mp_obj_new_bytes(buf, len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bluetooth_gatts_read_obj, bluetooth_gatts_read);

STATIC mp_obj_t bluetooth_gatts_write(mp_obj_t self_in, mp_obj_t value_handle_in, mp_obj_t data) {
    mp_buffer_info_t bufinfo = {0};
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
    size_t len = bufinfo.len;
    int err = mp_bt_chr_value_write(mp_obj_get_int(value_handle_in), bufinfo.buf, &len);
    if (err != 0) {
        mp_raise_OSError(err);
    }
    return MP_OBJ_NEW_SMALL_INT(len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(bluetooth_gatts_write_obj, bluetooth_gatts_write);

STATIC mp_obj_t bluetooth_gatts_notify(size_t n_args, const mp_obj_t *args) {
    mp_int_t value_handle = mp_obj_get_int(args[1]);
    mp_int_t conn_handle = mp_obj_get_int(args[2]);

    if (n_args == 4) {
        mp_buffer_info_t bufinfo = {0};
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
        size_t len = bufinfo.len;
        int err = mp_bt_chr_value_notify_send(value_handle, conn_handle, bufinfo.buf, &len);
        if (err != 0) {
            mp_raise_OSError(err);
        }
        return MP_OBJ_NEW_SMALL_INT(len);
    } else {
        int err = mp_bt_chr_value_notify(value_handle, conn_handle);
        return bluetooth_handle_errno(err);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bluetooth_gatts_notify_obj, 3, 4, bluetooth_gatts_notify);

// ----------------------------------------------------------------------------
// Bluetooth object: GATTC (Central/Scanner role)
// ----------------------------------------------------------------------------

STATIC mp_obj_t bluetooth_gattc_disc_svcs(mp_obj_t self_in, mp_obj_t conn_handle_in) {
    mp_int_t conn_handle = mp_obj_get_int(conn_handle_in);
    return bluetooth_handle_errno(mp_bt_peripheral_disc_primary_svcs(conn_handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bluetooth_gattc_disc_svcs_obj, bluetooth_gattc_disc_svcs);

STATIC mp_obj_t bluetooth_gattc_disc_chrs(size_t n_args, const mp_obj_t *args) {
    mp_int_t start_handle = mp_obj_get_int(args[1]);
    mp_int_t end_handle = mp_obj_get_int(args[2]);
    mp_int_t conn_handle = mp_obj_get_int(args[3]);
    return bluetooth_handle_errno(mp_bt_peripheral_disc_chrs(start_handle, end_handle, conn_handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bluetooth_gattc_disc_chrs_obj, 4, 4, bluetooth_gattc_disc_chrs);

STATIC mp_obj_t bluetooth_gattc_disc_dscs(size_t n_args, const mp_obj_t *args) {
    mp_int_t start_handle = mp_obj_get_int(args[1]);
    mp_int_t end_handle = mp_obj_get_int(args[2]);
    mp_int_t conn_handle = mp_obj_get_int(args[3]);
    return bluetooth_handle_errno(mp_bt_peripheral_disc_dscs(start_handle, end_handle, conn_handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bluetooth_gattc_disc_dscs_obj, 4, 4, bluetooth_gattc_disc_dscs);

STATIC mp_obj_t bluetooth_gattc_read(mp_obj_t self_in, mp_obj_t value_handle_in, mp_obj_t conn_handle_in) {
    // TODO: Think about ordering of value_handle, conn_handle.
    // Currently matches gatts_notify, which has this order because gatts_write only takes a value_handle.
    mp_int_t value_handle = mp_obj_get_int(value_handle_in);
    mp_int_t conn_handle = mp_obj_get_int(conn_handle_in);
    return bluetooth_handle_errno(mp_bt_peripheral_read_chr(value_handle, conn_handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(bluetooth_gattc_read_obj, bluetooth_gattc_read);

STATIC mp_obj_t bluetooth_gattc_write(size_t n_args, const mp_obj_t *args) {
    mp_int_t value_handle = mp_obj_get_int(args[1]);
    mp_int_t conn_handle = mp_obj_get_int(args[2]);
    mp_obj_t data = args[3];
    mp_buffer_info_t bufinfo = {0};
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
    size_t len = bufinfo.len;
    return bluetooth_handle_errno(mp_bt_peripheral_write_chr(value_handle, conn_handle, bufinfo.buf, &len));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bluetooth_gattc_write_obj, 4, 4, bluetooth_gattc_write);

// ----------------------------------------------------------------------------
// Bluetooth object: Definition
// ----------------------------------------------------------------------------

STATIC const mp_rom_map_elem_t bluetooth_locals_dict_table[] = {
    // General
    { MP_ROM_QSTR(MP_QSTR_active), MP_ROM_PTR(&bluetooth_active_obj) },
    { MP_ROM_QSTR(MP_QSTR_config), MP_ROM_PTR(&bluetooth_config_obj) },
    { MP_ROM_QSTR(MP_QSTR_irq), MP_ROM_PTR(&bluetooth_irq_obj) },
    // GAP
    // TODO: Potentially rename these to gap_*.
    { MP_ROM_QSTR(MP_QSTR_advertise), MP_ROM_PTR(&bluetooth_advertise_obj) },
#if MP_BT_ENABLE_CENTRAL_MODE
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&bluetooth_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&bluetooth_scan_obj) },
#endif
    { MP_ROM_QSTR(MP_QSTR_disconnect), MP_ROM_PTR(&bluetooth_disconnect_obj) },
    // GATT Server (i.e. peripheral/advertiser role)
    { MP_ROM_QSTR(MP_QSTR_gatts_add_svc), MP_ROM_PTR(&bluetooth_gatts_add_svc_obj) },
    { MP_ROM_QSTR(MP_QSTR_gatts_read), MP_ROM_PTR(&bluetooth_gatts_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_gatts_write), MP_ROM_PTR(&bluetooth_gatts_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_gatts_notify), MP_ROM_PTR(&bluetooth_gatts_notify_obj) },
#if MP_BT_ENABLE_CENTRAL_MODE
    // GATT Client (i.e. central/scanner role)
    { MP_ROM_QSTR(MP_QSTR_gattc_disc_svcs), MP_ROM_PTR(&bluetooth_gattc_disc_svcs_obj) },
    { MP_ROM_QSTR(MP_QSTR_gattc_disc_chrs), MP_ROM_PTR(&bluetooth_gattc_disc_chrs_obj) },
    { MP_ROM_QSTR(MP_QSTR_gattc_disc_dscs), MP_ROM_PTR(&bluetooth_gattc_disc_dscs_obj) },
    { MP_ROM_QSTR(MP_QSTR_gattc_read), MP_ROM_PTR(&bluetooth_gattc_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_gattc_write), MP_ROM_PTR(&bluetooth_gattc_write_obj) },
#endif
};
STATIC MP_DEFINE_CONST_DICT(bluetooth_locals_dict, bluetooth_locals_dict_table);

STATIC const mp_obj_type_t bluetooth_type = {
    { &mp_type_type },
    .name = MP_QSTR_Bluetooth,
    .make_new = bluetooth_make_new,
    .locals_dict = (void*)&bluetooth_locals_dict,
};

STATIC const mp_rom_map_elem_t mp_module_bluetooth_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bluetooth) },
    { MP_ROM_QSTR(MP_QSTR_Bluetooth), MP_ROM_PTR(&bluetooth_type) },
    { MP_ROM_QSTR(MP_QSTR_UUID), MP_ROM_PTR(&uuid_type) },
    { MP_ROM_QSTR(MP_QSTR_FLAG_READ), MP_ROM_INT(MP_BT_CHR_FLAG_READ) },
    { MP_ROM_QSTR(MP_QSTR_FLAG_WRITE), MP_ROM_INT(MP_BT_CHR_FLAG_WRITE) },
    { MP_ROM_QSTR(MP_QSTR_FLAG_NOTIFY), MP_ROM_INT(MP_BT_CHR_FLAG_NOTIFY) },
    // So much QSTR ROM for these IRQ event names...... (~200 bytes!)
    // { MP_ROM_QSTR(MP_QSTR_IRQ_CENTRAL_CONNECT), MP_ROM_INT(MP_BT_IRQ_CENTRAL_CONNECT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_CENTRAL_DISCONNECT), MP_ROM_INT(MP_BT_IRQ_CENTRAL_DISCONNECT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_CHR_WRITE), MP_ROM_INT(MP_BT_IRQ_CHR_WRITE) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_SCAN_RESULT), MP_ROM_INT(MP_BT_IRQ_SCAN_RESULT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_SCAN_COMPLETE), MP_ROM_INT(MP_BT_IRQ_SCAN_COMPLETE) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_PERIPHERAL_CONNECT), MP_ROM_INT(MP_BT_IRQ_PERIPHERAL_CONNECT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_PERIPHERAL_DISCONNECT), MP_ROM_INT(MP_BT_IRQ_PERIPHERAL_DISCONNECT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_PERIPHERAL_SVC_RESULT), MP_ROM_INT(MP_BT_IRQ_PERIPHERAL_SVC_RESULT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_PERIPHERAL_CHR_RESULT), MP_ROM_INT(MP_BT_IRQ_PERIPHERAL_CHR_RESULT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_PERIPHERAL_DSC_RESULT), MP_ROM_INT(MP_BT_IRQ_PERIPHERAL_DSC_RESULT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_PERIPHERAL_READ_RESULT), MP_ROM_INT(MP_BT_IRQ_PERIPHERAL_READ_RESULT) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_PERIPHERAL_WRITE_STATUS), MP_ROM_INT(MP_BT_IRQ_PERIPHERAL_WRITE_STATUS) },
    // { MP_ROM_QSTR(MP_QSTR_IRQ_ALL), MP_ROM_INT(MP_BT_IRQ_ALL) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_bluetooth_globals, mp_module_bluetooth_globals_table);

const mp_obj_module_t mp_module_bluetooth = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_bluetooth_globals,
};

// Helpers

#if !MP_BT_CALLBACK_ALLOC
STATIC mp_obj_t bluetooth_invoke_irq(mp_obj_t none_in) {
    // This is always executing in schedule context.
    while (1) {
        mp_obj_t handler = mp_const_none;
        mp_int_t event = 0;
        mp_obj_t data = mp_const_none;

        mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
        mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));

        event = ringbuf_get16(&o->ringbuf);
        if (event < 0) {
            // Nothing available in ringbuf.
            MICROPY_END_ATOMIC_SECTION(atomic_state);
            break;
        }

        handler = o->irq_handler;

        if (event == MP_BT_IRQ_CENTRAL_CONNECT || event == MP_BT_IRQ_PERIPHERAL_CONNECT) {
            // addr_type, addr, conn_handle
            assert(ringbuf_avail(&o->ringbuf) >= 9);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(3, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get(&o->ringbuf));
            uint8_t addr[6];
            for (int i = 0; i < 6; ++i) {
                addr[i] = ringbuf_get(&o->ringbuf);
            }
            data_tuple->items[1] = mp_obj_new_bytes(addr, MP_ARRAY_SIZE(addr));
            data_tuple->items[2] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data = MP_OBJ_FROM_PTR(data_tuple);
        } else if (event == MP_BT_IRQ_CENTRAL_DISCONNECT || event == MP_BT_IRQ_PERIPHERAL_DISCONNECT) {
            // conn_handle
            assert(ringbuf_avail(&o->ringbuf) >= 2);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(1, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data = MP_OBJ_FROM_PTR(data_tuple);
        } else if (event == MP_BT_IRQ_CHR_WRITE) {
            // value_handle, conn_handle
            assert(ringbuf_avail(&o->ringbuf) >= 4);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(2, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[1] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data = MP_OBJ_FROM_PTR(data_tuple);
#if MP_BT_ENABLE_CENTRAL_MODE
        } else if (event == MP_BT_IRQ_SCAN_RESULT) {
            // addr_type, addr, connectable, rssi, adv_data
            assert(ringbuf_avail(&o->ringbuf) >= 8);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(5, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get(&o->ringbuf));
            uint8_t addr[6];
            for (int i = 0; i < 6; ++i) {
                addr[i] = ringbuf_get(&o->ringbuf);
            }
            data_tuple->items[1] = mp_obj_new_bytes(addr, MP_ARRAY_SIZE(addr));
            data_tuple->items[2] = mp_obj_new_bool(ringbuf_get(&o->ringbuf));
            data_tuple->items[3] = MP_OBJ_NEW_SMALL_INT(ringbuf_get(&o->ringbuf));
            size_t adv_data_len = ringbuf_get(&o->ringbuf);
            assert(ringbuf_avail(&o->ringbuf) >= adv_data_len);
            uint8_t *adv_data = m_new(uint8_t, adv_data_len);
            for (int i = 0; i < adv_data_len; ++i) {
                adv_data[i] = ringbuf_get(&o->ringbuf);
            }
            data_tuple->items[4] = mp_obj_new_bytes(adv_data, adv_data_len);
            data = MP_OBJ_FROM_PTR(data_tuple);
        } else if (event == MP_BT_IRQ_SCAN_COMPLETE) {
            // No params required.
        } else if (event == MP_BT_IRQ_PERIPHERAL_SVC_RESULT) {
            // start_handle, end_handle, uuid, conn_handle
            assert(ringbuf_avail(&o->ringbuf) >= 7);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(4, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[1] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[2] = MP_OBJ_FROM_PTR(ringbuf_get_uuid(&o->ringbuf));
            data_tuple->items[3] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data = MP_OBJ_FROM_PTR(data_tuple);
        } else if (event == MP_BT_IRQ_PERIPHERAL_CHR_RESULT) {
            // def_handle, value_handle, properties, uuid, conn_handle
            assert(ringbuf_avail(&o->ringbuf) >= 8);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(5, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[1] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[2] = MP_OBJ_NEW_SMALL_INT(ringbuf_get(&o->ringbuf));
            data_tuple->items[3] = MP_OBJ_FROM_PTR(ringbuf_get_uuid(&o->ringbuf));
            data_tuple->items[4] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data = MP_OBJ_FROM_PTR(data_tuple);
        } else if (event == MP_BT_IRQ_PERIPHERAL_DSC_RESULT) {
            // handle, uuid, conn_handle
            assert(ringbuf_avail(&o->ringbuf) >= 5);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(3, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[1] = MP_OBJ_FROM_PTR(ringbuf_get_uuid(&o->ringbuf));
            data_tuple->items[2] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data = MP_OBJ_FROM_PTR(data_tuple);
        } else if (event == MP_BT_IRQ_PERIPHERAL_READ_RESULT) {
            // value_handle, conn_handle, data
            assert(ringbuf_avail(&o->ringbuf) >= 5);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(3, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[1] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            uint8_t chr_data_len = ringbuf_get(&o->ringbuf);
            uint8_t *chr_data = m_new(uint8_t, chr_data_len);
            for (int i = 0; i < chr_data_len; ++i) {
                chr_data[i] = ringbuf_get(&o->ringbuf);
            }
            data_tuple->items[2] = mp_obj_new_bytes(chr_data, chr_data_len);
            data = MP_OBJ_FROM_PTR(data_tuple);
        } else if (event == MP_BT_IRQ_PERIPHERAL_WRITE_STATUS) {
            // value_handle, conn_handle, status
            assert(ringbuf_avail(&o->ringbuf) >= 6);
            mp_obj_tuple_t *data_tuple = mp_obj_new_tuple(3, NULL);
            data_tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[1] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data_tuple->items[2] = MP_OBJ_NEW_SMALL_INT(ringbuf_get16(&o->ringbuf));
            data = MP_OBJ_FROM_PTR(data_tuple);
#endif // MP_BT_ENABLE_CENTRAL_MODE
        }

        MICROPY_END_ATOMIC_SECTION(atomic_state);

        mp_call_function_2(handler, MP_OBJ_NEW_SMALL_INT(event), data);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bluetooth_invoke_irq_obj, bluetooth_invoke_irq);
#endif // !MP_BT_CALLBACK_ALLOC

// ----------------------------------------------------------------------------
// Port API
// ----------------------------------------------------------------------------

#if MP_BT_CALLBACK_ALLOC

// TODO: Direct call of IRQ handler. Construct relevant tuples for each event type.

STATIC void mp_bt_connected_common(uint16_t event, uint16_t conn_handle, uint8_t addr_type, const uint8_t *addr) {
}

STATIC void mp_bt_disconnected_common(uint16_t event, uint16_t conn_handle) {
}

void mp_bt_central_connected(uint16_t conn_handle, uint8_t addr_type, const uint8_t *addr) {
    mp_bt_connected_common(MP_BT_IRQ_CENTRAL_CONNECT, conn_handle, addr_type, addr);
}

void mp_bt_central_disconnected(uint16_t conn_handle) {
    mp_bt_disconnected_common(MP_BT_IRQ_CENTRAL_DISCONNECT, conn_handle);
}

void mp_bt_chr_on_write(uint16_t value_handle, uint16_t conn_handle) {
}

#if MP_BT_ENABLE_CENTRAL_MODE
void mp_bt_scan_complete(void) {
}

void mp_bt_scan_result(uint8_t addr_type, const uint8_t *addr, bool connectable, const int8_t rssi, const uint8_t *data, size_t data_len) {
}

void mp_bt_peripheral_connected(uint16_t conn_handle, uint8_t addr_type, const uint8_t *addr) {
}

void mp_bt_peripheral_disconnected(uint16_t conn_handle) {
}

void mp_bt_peripheral_primary_svc_result(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle, mp_obj_bt_uuid_t *svc_uuid) {
}

void mp_bt_peripheral_chr_result(uint16_t conn_handle, uint16_t def_handle, uint16_t value_handle, uint8_t properties, mp_obj_bt_uuid_t *chr_uuid) {
}

void mp_bt_peripheral_dsc_result(uint16_t conn_handle, conn_handle, uint16_t conn_handle, handle, mp_obj_bt_uuid_t *dsc_uuid) {

}

void mp_bt_peripheral_chr_read_result(uint16_t conn_handle, uint16_t value_handle, const uint8_t *data, size_t data_len) {
}

void mp_bt_peripheral_chr_write_status(uint16_t conn_handle, uint16_t value_handle, uint16_t status) {
}

#endif // MP_BT_ENABLE_CENTRAL_MODE

#else

// Callbacks are called in interrupt context (i.e. can't allocate), so we need to push the data
// into the ringbuf and schedule the callback via mp_sched_schedule.

STATIC bool enqueue_irq(mp_obj_bluetooth_t *o, size_t len, uint16_t event, bool *sched) {
    *sched = false;

    if (ringbuf_free(&o->ringbuf) >= len + 2 && (o->irq_trigger & event) && o->irq_handler != mp_const_none) {
        *sched = ringbuf_avail(&o->ringbuf) == 0;
        ringbuf_put16(&o->ringbuf, event);
        return true;
    } else {
        return false;
    }
}

STATIC void mp_bt_connected_common(uint16_t event, uint16_t conn_handle, uint8_t addr_type, const uint8_t *addr) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 9, event, &sched)) {
        ringbuf_put(&o->ringbuf, addr_type);
        for (int i = 0; i < 6; ++i) {
            ringbuf_put(&o->ringbuf, addr[i]);
        }
        ringbuf_put16(&o->ringbuf, conn_handle);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

STATIC void mp_bt_disconnected_common(uint16_t event, uint16_t conn_handle) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 2, event, &sched)) {
        ringbuf_put16(&o->ringbuf, conn_handle);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

void mp_bt_central_connected(uint16_t conn_handle, uint8_t addr_type, const uint8_t *addr) {
    mp_bt_connected_common(MP_BT_IRQ_CENTRAL_CONNECT, conn_handle, addr_type, addr);
}

void mp_bt_central_disconnected(uint16_t conn_handle) {
    mp_bt_disconnected_common(MP_BT_IRQ_CENTRAL_DISCONNECT, conn_handle);
}

void mp_bt_chr_on_write(uint16_t value_handle, uint16_t conn_handle) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 4, MP_BT_IRQ_CHR_WRITE, &sched)) {
        ringbuf_put16(&o->ringbuf, value_handle);
        ringbuf_put16(&o->ringbuf, conn_handle);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

#if MP_BT_ENABLE_CENTRAL_MODE
void mp_bt_scan_complete(void) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 0, MP_BT_IRQ_SCAN_COMPLETE, &sched)) {
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

void mp_bt_scan_result(uint8_t addr_type, const uint8_t *addr, bool connectable, const int8_t rssi, const uint8_t *data, size_t data_len) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 1 + 6 + 1 + 1 + data_len, MP_BT_IRQ_SCAN_RESULT, &sched)) {
        ringbuf_put(&o->ringbuf, addr_type);
        for (int i = 0; i < 6; ++i) {
            ringbuf_put(&o->ringbuf, addr[i]);
        }
        ringbuf_put(&o->ringbuf, connectable ? 1 : 0);
        ringbuf_put(&o->ringbuf, rssi);
        ringbuf_put(&o->ringbuf, data_len);
        for (int i = 0; i < data_len; ++i) {
            ringbuf_put(&o->ringbuf, data[i]);
        }
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

void mp_bt_peripheral_connected(uint16_t conn_handle, uint8_t addr_type, const uint8_t *addr) {
    mp_bt_connected_common(MP_BT_IRQ_PERIPHERAL_CONNECT, conn_handle, addr_type, addr);
}

void mp_bt_peripheral_disconnected(uint16_t conn_handle) {
    mp_bt_disconnected_common(MP_BT_IRQ_PERIPHERAL_DISCONNECT, conn_handle);
}

void mp_bt_peripheral_primary_svc_result(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle, mp_obj_bt_uuid_t *svc_uuid) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 2 + 2 + 2 + 1 + svc_uuid->type, MP_BT_IRQ_PERIPHERAL_SVC_RESULT, &sched)) {
        ringbuf_put16(&o->ringbuf, start_handle);
        ringbuf_put16(&o->ringbuf, end_handle);
        ringbuf_put_uuid(&o->ringbuf, svc_uuid);
        ringbuf_put16(&o->ringbuf, conn_handle);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

void mp_bt_peripheral_chr_result(uint16_t conn_handle, uint16_t def_handle, uint16_t value_handle, uint8_t properties, mp_obj_bt_uuid_t *chr_uuid) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 2 + 2 + 2 + 1 + chr_uuid->type, MP_BT_IRQ_PERIPHERAL_CHR_RESULT, &sched)) {
        ringbuf_put16(&o->ringbuf, def_handle);
        ringbuf_put16(&o->ringbuf, value_handle);
        ringbuf_put(&o->ringbuf, properties);
        ringbuf_put_uuid(&o->ringbuf, chr_uuid);
        ringbuf_put16(&o->ringbuf, conn_handle);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

void mp_bt_peripheral_dsc_result(uint16_t conn_handle, uint16_t handle, mp_obj_bt_uuid_t *dsc_uuid) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 2 + 2 + 1 + dsc_uuid->type, MP_BT_IRQ_PERIPHERAL_DSC_RESULT, &sched)) {
        ringbuf_put16(&o->ringbuf, handle);
        ringbuf_put_uuid(&o->ringbuf, dsc_uuid);
        ringbuf_put16(&o->ringbuf, conn_handle);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

void mp_bt_peripheral_chr_read_result(uint16_t conn_handle, uint16_t value_handle, const uint8_t *data, size_t data_len) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 2 + 2 + 1 + data_len, MP_BT_IRQ_PERIPHERAL_READ_RESULT, &sched)) {
        ringbuf_put16(&o->ringbuf, value_handle);
        ringbuf_put16(&o->ringbuf, conn_handle);
        ringbuf_put(&o->ringbuf, data_len);
        for (int i = 0; i < data_len; ++i) {
        ringbuf_put(&o->ringbuf, data[i]);
        }
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}

void mp_bt_peripheral_chr_write_status(uint16_t conn_handle, uint16_t value_handle, uint16_t status) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_obj_bluetooth_t *o = MP_OBJ_TO_PTR(MP_STATE_VM(bluetooth));
    bool sched;
    if (enqueue_irq(o, 2 + 2 + 2, MP_BT_IRQ_PERIPHERAL_WRITE_STATUS, &sched)) {
        ringbuf_put16(&o->ringbuf, value_handle);
        ringbuf_put16(&o->ringbuf, conn_handle);
        ringbuf_put16(&o->ringbuf, status);
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    if (sched) {
        mp_sched_schedule(MP_OBJ_FROM_PTR(MP_ROM_PTR(&bluetooth_invoke_irq_obj)), mp_const_none);
    }
}
#endif // MP_BT_ENABLE_CENTRAL_MODE

#endif // else MP_BT_CALLBACK_ALLOC

#endif //MICROPY_PY_BLUETOOTH
