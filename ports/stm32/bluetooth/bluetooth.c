/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Damien P. George
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
#include "py/mperrno.h"
#include "py/mphal.h"

#if MICROPY_PY_BLUETOOTH

#include "extmod/modbluetooth.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"
#include "transport/uart/ble_hci_uart.h"

STATIC int8_t ble_hs_err_to_errno_table[] = {
    [BLE_HS_EAGAIN]             = MP_EAGAIN,
    [BLE_HS_EALREADY]           = MP_EALREADY,
    [BLE_HS_EINVAL]             = MP_EINVAL,
    [BLE_HS_EMSGSIZE]           = MP_EIO,
    [BLE_HS_ENOENT]             = MP_ENOENT,
    [BLE_HS_ENOMEM]             = MP_ENOMEM,
    [BLE_HS_ENOTCONN]           = MP_ENOTCONN,
    [BLE_HS_ENOTSUP]            = MP_EOPNOTSUPP,
    [BLE_HS_EAPP]               = MP_EIO,
    [BLE_HS_EBADDATA]           = MP_EIO,
    [BLE_HS_EOS]                = MP_EIO,
    [BLE_HS_ECONTROLLER]        = MP_EIO,
    [BLE_HS_ETIMEOUT]           = MP_ETIMEDOUT,
    [BLE_HS_EDONE]              = MP_EIO,  // TODO: Maybe should be MP_EISCONN (connect uses this for "already connected").
    [BLE_HS_EBUSY]              = MP_EBUSY,
    [BLE_HS_EREJECT]            = MP_EIO,
    [BLE_HS_EUNKNOWN]           = MP_EIO,
    [BLE_HS_EROLE]              = MP_EIO,
    [BLE_HS_ETIMEOUT_HCI]       = MP_EIO,
    [BLE_HS_ENOMEM_EVT]         = MP_EIO,
    [BLE_HS_ENOADDR]            = MP_EIO,
    [BLE_HS_ENOTSYNCED]         = MP_EIO,
    [BLE_HS_EAUTHEN]            = MP_EIO,
    [BLE_HS_EAUTHOR]            = MP_EIO,
    [BLE_HS_EENCRYPT]           = MP_EIO,
    [BLE_HS_EENCRYPT_KEY_SZ]    = MP_EIO,
    [BLE_HS_ESTORE_CAP]         = MP_EIO,
    [BLE_HS_ESTORE_FAIL]        = MP_EIO,
    [BLE_HS_EPREEMPTED]         = MP_EIO,
    [BLE_HS_EDISABLED]          = MP_EIO,
};

STATIC int ble_hs_err_to_errno(int err) {
    if (0 <= err && err < MP_ARRAY_SIZE(ble_hs_err_to_errno_table)) {
        return ble_hs_err_to_errno_table[err];
    } else {
        return MP_EIO;
    }
}

// Allocate and store as root pointer.
// TODO: This is duplicated from mbedtls.
//       Perhaps make this a generic feature?
void *m_malloc_bluetooth(size_t size) {
    void **ptr = m_malloc0(size + 2 * sizeof(uintptr_t));
    if (MP_STATE_PORT(mbedtls_memory) != NULL) {
        MP_STATE_PORT(mbedtls_memory)[0] = ptr;
    }
    ptr[0] = NULL;
    ptr[1] = MP_STATE_PORT(mbedtls_memory);
    MP_STATE_PORT(mbedtls_memory) = ptr;
    return &ptr[2];
}

#define m_new_bluetooth(type, num) ((type*)m_malloc_bluetooth(sizeof(type) * (num)))

// void m_free_bluetooth(void *ptr_in) {
//     void **ptr = &((void**)ptr_in)[-2];
//     if (ptr[1] != NULL) {
//         ((void**)ptr[1])[0] = ptr[0];
//     }
//     if (ptr[0] != NULL) {
//         ((void**)ptr[0])[1] = ptr[1];
//     } else {
//         MP_STATE_PORT(mbedtls_memory) = ptr[1];
//     }
//     m_free(ptr);
// }

STATIC ble_uuid_t* create_nimble_uuid(const mp_obj_bt_uuid_t *uuid) {
    if (uuid->type == MP_BT_UUID_TYPE_16) {
        ble_uuid16_t *result = m_new(ble_uuid16_t, 1);
        result->u.type = BLE_UUID_TYPE_16;
        result->value = uuid->uuid16;
        return (ble_uuid_t*)result;
    } else if (uuid->type == MP_BT_UUID_TYPE_32) {
        ble_uuid32_t *result = m_new(ble_uuid32_t, 1);
        result->u.type = BLE_UUID_TYPE_32;
        result->value = uuid->uuid32;
        return (ble_uuid_t*)result;
    } else if (uuid->type == MP_BT_UUID_TYPE_128) {
        ble_uuid128_t *result = m_new(ble_uuid128_t, 1);
        result->u.type = BLE_UUID_TYPE_128;
        memcpy(result->value, uuid->uuid128, 16);
        return (ble_uuid_t*)result;
    } else {
        return NULL;
    }
}

STATIC mp_obj_bt_uuid_t create_mp_uuid(const ble_uuid_any_t *uuid) {
    mp_obj_bt_uuid_t result;
    switch (uuid->u.type) {
        case BLE_UUID_TYPE_16:
            result.type = MP_BT_UUID_TYPE_16;
            result.uuid16 = uuid->u16.value;
            break;
        case BLE_UUID_TYPE_32:
            result.type = MP_BT_UUID_TYPE_32;
            result.uuid32 = uuid->u32.value;
            break;
        case BLE_UUID_TYPE_128:
            result.type = MP_BT_UUID_TYPE_128;
            memcpy(result.uuid128, uuid->u128.value, 16);
            break;
        default:
            assert(false);
    }
    return result;
}

STATIC ble_addr_t create_nimble_addr(uint8_t addr_type, const uint8_t *addr) {
    ble_addr_t addr_nimble;
    addr_nimble.type = addr_type;
    memcpy(addr_nimble.val, addr, 6);
    return addr_nimble;
}

STATIC mp_map_t *gatts_db = MP_OBJ_NULL;

typedef struct {
    uint8_t data[MP_BT_MAX_ATTR_SIZE];
    uint8_t data_len;
} gatts_db_entry_t;

/******************************************************************************/
// RUN LOOP

enum {
    BLE_STATE_OFF,
    BLE_STATE_STARTING,
    BLE_STATE_ACTIVE,
};

static volatile int ble_state = BLE_STATE_OFF;

extern void nimble_uart_process(void);
extern void os_eventq_run_all(void);
extern void os_callout_process(void);

// hook for network poller to run this periodically
void nimble_poll(void) {
    if (ble_state == BLE_STATE_OFF) {
        return;
    }

    nimble_uart_process();
    os_callout_process();
    os_eventq_run_all();
}

/******************************************************************************/
// BINDINGS

STATIC void reset_cb(int reason) {
    (void)reason;
}

STATIC void sync_cb(void) {
    ble_hs_util_ensure_addr(0); // prefer public address
    ble_svc_gap_device_name_set("PYBD");

    ble_state = BLE_STATE_ACTIVE;
}

STATIC void gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            printf("gatts_register_cb: svc uuid=%p handle=%d\n", &ctxt->svc.svc_def->uuid, ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            printf("gatts_register_cb: chr uuid=%p def_handle=%d val_handle=%d\n", &ctxt->chr.chr_def->uuid, ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            printf("gatts_register_cb: dsc uuid=%p handle=%d\n", &ctxt->dsc.dsc_def->uuid, ctxt->dsc.handle);
            break;

        default:
            printf("gatts_register_cb: unknown op %d\n", ctxt->op);
            break;
    }
}

STATIC int gap_event_cb(struct ble_gap_event *event, void *arg) {
    // printf("gap_event_cb: type=%d\n", event->type);
    struct ble_gap_conn_desc desc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (event->connect.status == 0) {
                // Connection established.
                mp_bt_central_connected(event->connect.conn_handle, desc.peer_id_addr.type, desc.peer_id_addr.val);
            } else {
                // Connection failed.
                mp_bt_central_disconnected(event->connect.conn_handle);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            // Disconnect.
            // TODO: Include address? event->disconnect.conn.peer_id_addr.val
            mp_bt_central_disconnected(event->disconnect.conn.conn_handle);

            break;
    }

    return 0;
}

int mp_bt_enable(void) {
    if (ble_state != BLE_STATE_OFF) {
        return 0;
    }

    ble_state = BLE_STATE_STARTING;

    ble_hs_cfg.reset_cb = reset_cb;
    ble_hs_cfg.sync_cb = sync_cb;
    ble_hs_cfg.gatts_register_cb = gatts_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hci_uart_init();
    nimble_port_init();
    ble_hs_sched_start();

    // Wait for sync callback
    while (ble_state != BLE_STATE_ACTIVE) {
        MICROPY_EVENT_POLL_HOOK
    }

    return 0;
}

void mp_bt_disable(void) {
    ble_state = BLE_STATE_OFF;
    mp_hal_pin_low(pyb_pin_BT_REG_ON);
}

bool mp_bt_is_enabled(void) {
    return ble_state == BLE_STATE_ACTIVE;
}

void mp_bt_get_addr(uint8_t *addr) {
    mp_hal_get_mac(MP_HAL_MAC_BDADDR, addr);
    // TODO: need to convert MSB/LSB?
}

int mp_bt_advertise_start(mp_bt_adv_type_t type, uint16_t interval_ms, const uint8_t *adv_data, size_t adv_data_len, const uint8_t *sr_data, size_t sr_data_len) {
    int ret;

    mp_bt_advertise_stop();

    if (adv_data != NULL) {
        ret = ble_gap_adv_set_data(adv_data, adv_data_len);
        if (ret != 0) {
            //printf("ble_gap_adv_set_data: fail with %u\n", ret);
            return ble_hs_err_to_errno(ret);
        }
    }

    if (sr_data != NULL) {
        ret = ble_gap_adv_rsp_set_data(sr_data, sr_data_len);
        if (ret != 0) {
            //printf("ble_gap_adv_rsp_set_data: fail with %u\n", ret);
            return ble_hs_err_to_errno(ret);
        }
    }

    // Convert from 1ms to 0.625ms units.
    interval_ms = interval_ms * 8 / 5;
    if (interval_ms < 0x20 || interval_ms > 0x4000) {
        return MP_EINVAL;
    }

    struct ble_gap_adv_params adv_params = {
        .conn_mode = type,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = interval_ms,
        .itvl_max = interval_ms,
        .channel_map = 7, // all 3 channels
    };

    ret = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
    if (ret != 0) {
        //printf("ble_gap_adv_start: fail with %u\n", ret);
        return ble_hs_err_to_errno(ret);
    }

    return 0;
}

void mp_bt_advertise_stop(void) {
    if (ble_gap_adv_active()) {
        ble_gap_adv_stop();
    }
}

static int chr_access_cb(uint16_t conn_handle, uint16_t value_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // printf("chr_access_cb: conn_handle=%u value_handle=%u op=%u\n", conn_handle, value_handle, ctxt->op);
    mp_map_elem_t *elem;
    gatts_db_entry_t *entry;
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            // printf("read_chr\n");

            elem = mp_map_lookup(gatts_db, MP_OBJ_NEW_SMALL_INT(value_handle), MP_MAP_LOOKUP);
            if (!elem) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            entry = MP_OBJ_TO_PTR(elem->value);
            os_mbuf_append(ctxt->om, entry->data, entry->data_len);

            return 0;
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            for (struct os_mbuf *om = ctxt->om; om; om = SLIST_NEXT(om, om_next)) {
                // printf("write_chr: data='%.*s'\n", om->om_len, om->om_data);
            }

            elem = mp_map_lookup(gatts_db, MP_OBJ_NEW_SMALL_INT(value_handle), MP_MAP_LOOKUP);
            if (!elem) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            entry = MP_OBJ_TO_PTR(elem->value);
            entry->data_len = MIN(MP_BT_MAX_ATTR_SIZE, OS_MBUF_PKTLEN(ctxt->om));
            os_mbuf_copydata(ctxt->om, 0, entry->data_len, entry->data);

            mp_bt_chr_on_write(value_handle, conn_handle);
            return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

int mp_bt_add_svc(mp_obj_bt_uuid_t *svc_uuid, mp_obj_bt_uuid_t **chr_uuids, uint8_t *chr_flags, uint16_t *value_handles, size_t chr_len) {
    if (gatts_db == MP_OBJ_NULL) {
        gatts_db = m_new_bluetooth(mp_map_t, 1);
        mp_map_init(gatts_db, chr_len);
    }

    struct ble_gatt_chr_def *chr = m_new_bluetooth(struct ble_gatt_chr_def, chr_len + 1);
    for (size_t i = 0; i < chr_len; ++i) {
        chr[i].uuid = create_nimble_uuid(chr_uuids[i]);
        chr[i].access_cb = chr_access_cb;
        chr[i].arg = NULL;
        chr[i].descriptors = NULL;
        chr[i].flags = chr_flags[i];
        chr[i].min_key_size = 0;
        chr[i].val_handle = &value_handles[i];
    }
    chr[chr_len].uuid = NULL; // no more characteristic

    struct ble_gatt_svc_def *svc = m_new_bluetooth(struct ble_gatt_svc_def, 2);
    svc[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    svc[0].uuid = create_nimble_uuid(svc_uuid);
    svc[0].includes = NULL;
    svc[0].characteristics = chr;
    svc[1].type = 0; // no more services

    // Note: advertising must be stopped for gatts registration to work

    int ret;

    ret = ble_gatts_reset();
    if (ret != 0) {
        //printf("ble_gatts_reset: fail with %d\n", ret);
        return ble_hs_err_to_errno(ret);
    }

    ret = ble_gatts_count_cfg(svc);
    if (ret != 0) {
        //printf("ble_gatts_count_cfg: fail with %d\n", ret);
        return ble_hs_err_to_errno(ret);
    }

    ret = ble_gatts_add_svcs(svc);
    if (ret != 0) {
        //printf("ble_gatts_add_svcs: fail with %d\n", ret);
        return ble_hs_err_to_errno(ret);
    }

    ret = ble_gatts_start();
    if (ret != 0) {
        //printf("ble_gatts_start: fail with %d\n", ret);
        return ble_hs_err_to_errno(ret);
    }

    for (size_t i = 0; i < chr_len; ++i) {
        mp_map_elem_t *elem = mp_map_lookup(gatts_db, MP_OBJ_NEW_SMALL_INT(value_handles[i]), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
        elem->value = MP_OBJ_FROM_PTR(m_new0(gatts_db_entry_t, 1));
    }

    return 0;
}

int mp_bt_disconnect(uint16_t conn_handle) {
    return ble_hs_err_to_errno(ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM));
}

int mp_bt_chr_value_read(uint16_t value_handle, void *value, size_t *value_len) {
    mp_map_elem_t *elem = mp_map_lookup(gatts_db, MP_OBJ_NEW_SMALL_INT(value_handle), MP_MAP_LOOKUP);
    if (!elem) {
        return MP_EINVAL;
    }
    gatts_db_entry_t *entry = MP_OBJ_TO_PTR(elem->value);
    memcpy(value, entry->data, entry->data_len);
    *value_len = entry->data_len;
    return 0;
}

int mp_bt_chr_value_write(uint16_t value_handle, const void *value, size_t *value_len) {
    mp_map_elem_t *elem = mp_map_lookup(gatts_db, MP_OBJ_NEW_SMALL_INT(value_handle), MP_MAP_LOOKUP);
    if (!elem) {
        return MP_EINVAL;
    }
    gatts_db_entry_t *entry = MP_OBJ_TO_PTR(elem->value);
    entry->data_len = MIN(*value_len, MP_BT_MAX_ATTR_SIZE);
    memcpy(entry->data, value, entry->data_len);
    *value_len = entry->data_len;
    return 0;
}

// TODO: Could use ble_gatts_chr_updated to send to all subscribed centrals.

int mp_bt_chr_value_notify(uint16_t value_handle, uint16_t conn_handle) {
    // Confusingly, notify/notify_custom/indicate are "gattc" function (even though they're used by peripherals (i.e. gatt servers)).
    // See https://www.mail-archive.com/dev@mynewt.apache.org/msg01293.html
    return ble_hs_err_to_errno(ble_gattc_notify(conn_handle, value_handle));
    return 0;
}

int mp_bt_chr_value_notify_send(uint16_t value_handle, uint16_t conn_handle, const void *value, size_t *value_len) {
    struct os_mbuf *om = ble_hs_mbuf_from_flat(value, *value_len);
    if (om == NULL) {
        return -1;
    }
    // TODO: check that notify_custom takes ownership of om, if not os_mbuf_free_chain(om).
    return ble_hs_err_to_errno(ble_gattc_notify_custom(conn_handle, value_handle, om));
    return 0;
}

int mp_bt_chr_value_indicate(uint16_t value_handle, uint16_t conn_handle) {
    return ble_hs_err_to_errno(ble_gattc_indicate(conn_handle, value_handle));
    return 0;
}

STATIC int gap_scan_cb(struct ble_gap_event *event, void *arg) {
    // printf("scan callback %d\n", event->type);

    if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        mp_bt_scan_complete();
        return 0;
    }

    if (event->type != BLE_GAP_EVENT_DISC) {
        return 0;
    }

    // printf(" --> type %d\n", event->disc.event_type);


    if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND || event->disc.event_type ==  BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND) {
        bool connectable = event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND;
        mp_bt_scan_result(event->disc.addr.type, event->disc.addr.val, connectable, event->disc.rssi, event->disc.data, event->disc.length_data);
    } else if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
        // TODO: scan response.
    } else {
        printf("Unk scan: %d\n", event->disc.event_type);
    }

    // TODO: BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP

    return 0;
}

int mp_bt_scan_start(int32_t duration_ms) {
    if (duration_ms == 0) {
        duration_ms = BLE_HS_FOREVER;
    }
    STATIC const struct ble_gap_disc_params disc_params = {
        .itvl = BLE_GAP_SCAN_SLOW_INTERVAL1,
        .window = BLE_GAP_SCAN_SLOW_WINDOW1,
        .filter_policy = BLE_HCI_CONN_FILT_NO_WL,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 0,
    };
    int err = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, duration_ms, &disc_params, gap_scan_cb, NULL);
    return ble_hs_err_to_errno(err);
}

int mp_bt_scan_stop(void) {
    int err = ble_gap_disc_cancel();
    mp_bt_scan_complete();
    return ble_hs_err_to_errno(err);
}

// Central role: GAP events for a connected peripheral.
STATIC int peripheral_gap_event_cb(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (event->connect.status == 0) {
                // Connection established.
                mp_bt_peripheral_connected(event->connect.conn_handle, desc.peer_id_addr.type, desc.peer_id_addr.val);
            } else {
                // Connection failed.
                mp_bt_peripheral_disconnected(event->connect.conn_handle);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            // Disconnect.
            // TODO: Include address? event->disconnect.conn.peer_id_addr.val
            mp_bt_peripheral_disconnected(event->disconnect.conn.conn_handle);

            break;

        default:
            printf("unknown peripheral gap cb: %d\n", event->type);
            break;
    }
    return 0;
}

int mp_bt_peripheral_connect(uint8_t addr_type, const uint8_t *addr, int32_t duration_ms) {
    if (ble_gap_disc_active()) {
        mp_bt_scan_stop();
    }

    // TODO: This is the same as ble_gap_conn_params_dflt (i.e. passing NULL).
    STATIC const struct ble_gap_conn_params params = {
        .scan_itvl = 0x0010,
        .scan_window = 0x0010,
        .itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN,
        .itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX,
        .latency = BLE_GAP_INITIAL_CONN_LATENCY,
        .supervision_timeout = BLE_GAP_INITIAL_SUPERVISION_TIMEOUT,
        .min_ce_len = BLE_GAP_INITIAL_CONN_MIN_CE_LEN,
        .max_ce_len = BLE_GAP_INITIAL_CONN_MAX_CE_LEN,
    };

    ble_addr_t addr_nimble = create_nimble_addr(addr_type, addr);
    int err = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &addr_nimble, duration_ms, &params, &peripheral_gap_event_cb, NULL);
    return ble_hs_err_to_errno(err);
}

STATIC int peripheral_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc, void *arg) {
    //printf("service callback: %d %d %d\n", conn_handle, error->status, svc->start_handle);
    // TODO: Find out what error->status == 14 means (probably "end of services").
    if (error->status == 0) {
        mp_obj_bt_uuid_t svc_uuid = create_mp_uuid(&svc->uuid);
        mp_bt_peripheral_primary_svc_result(conn_handle, svc->start_handle, svc->end_handle, &svc_uuid);
    }
    return 0;
}

int mp_bt_peripheral_disc_primary_svcs(uint16_t conn_handle) {
    int err = ble_gattc_disc_all_svcs(conn_handle, &peripheral_disc_svc_cb, NULL);
    return ble_hs_err_to_errno(err);
}

STATIC int ble_gatt_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == 0) {
        mp_obj_bt_uuid_t chr_uuid = create_mp_uuid(&chr->uuid);
        mp_bt_peripheral_chr_result(conn_handle, chr->def_handle, chr->val_handle, chr->properties, &chr_uuid);
    }
    return 0;
}

int mp_bt_peripheral_disc_chrs(uint16_t start_handle, uint16_t end_handle, uint16_t conn_handle) {
    int err = ble_gattc_disc_all_chrs(conn_handle, start_handle, end_handle, &ble_gatt_chr_cb, NULL);
    return ble_hs_err_to_errno(err);
}

STATIC int ble_gatt_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg) {
    if (error->status == 0) {
        mp_obj_bt_uuid_t dsc_uuid = create_mp_uuid(&dsc->uuid);
        mp_bt_peripheral_dsc_result(conn_handle, dsc->handle, &dsc_uuid);
    }
    return 0;
}

int mp_bt_peripheral_disc_dscs(uint16_t start_handle, uint16_t end_handle, uint16_t conn_handle) {
    int err = ble_gattc_disc_all_dscs(conn_handle, start_handle, end_handle, &ble_gatt_dsc_cb, NULL);
    return ble_hs_err_to_errno(err);
}

STATIC int ble_gatt_attr_read_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
    // TODO: Maybe send NULL if error->status non-zero.
    if (error->status == 0) {
        uint8_t buf[MP_BT_MAX_ATTR_SIZE];
        size_t len = MIN(MP_BT_MAX_ATTR_SIZE, OS_MBUF_PKTLEN(attr->om));
        os_mbuf_copydata(attr->om, 0, len, buf);
        mp_bt_peripheral_chr_read_result(conn_handle, attr->handle, buf, len);
    }
    return 0;
}

// Initiate read of a value from the remote peripheral.
int mp_bt_peripheral_read_chr(uint16_t value_handle, uint16_t conn_handle) {
    int err = ble_gattc_read(conn_handle, value_handle, &ble_gatt_attr_read_cb, NULL);
    return ble_hs_err_to_errno(err);
}

STATIC int ble_gatt_attr_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
    mp_bt_peripheral_chr_write_status(conn_handle, attr->handle, error->status);
    return 0;
}

// Write the value to the remote peripheral.
int mp_bt_peripheral_write_chr(uint16_t value_handle, uint16_t conn_handle, const void *value, size_t *value_len) {
    int err = ble_gattc_write_flat(conn_handle, value_handle, value, *value_len, &ble_gatt_attr_write_cb, NULL);
    return ble_hs_err_to_errno(err);
}

#endif // MICROPY_PY_BLUETOOTH
