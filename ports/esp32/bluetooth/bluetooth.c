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

#if MICROPY_PY_BLUETOOTH

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#include "py/mperrno.h"
#include "py/runtime.h"
#include "extmod/modbluetooth.h"

// Semaphore to serialize asynchronous calls.
STATIC SemaphoreHandle_t mp_bt_call_complete;
STATIC esp_bt_status_t mp_bt_call_status;
STATIC union {
    // Ugly hack to return values from an event handler back to a caller.
    esp_gatt_if_t gatts_if;
    uint16_t      service_handle;
    uint16_t      attr_handle;
} mp_bt_call_result;

STATIC esp_ble_adv_type_t bluetooth_adv_type;
STATIC uint16_t bluetooth_adv_interval;
STATIC esp_gatt_if_t bluetooth_gatts_if;

STATIC void mp_bt_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
STATIC void mp_bt_gatts_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

STATIC const esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG
};
STATIC const uint8_t descr_value_buf[2];

// Convert an esp_err_t into an errno number.
STATIC int mp_bt_esp_errno(esp_err_t err) {
    switch (err) {
    case 0:
        return 0;
    case ESP_ERR_NO_MEM:
        return MP_ENOMEM;
    case ESP_ERR_INVALID_ARG:
        return MP_EINVAL;
    default:
        return MP_EPERM; // fallback
    }
}

// Convert the result of an asynchronous call to an errno value.
STATIC int mp_bt_status_errno(void) {
    switch (mp_bt_call_status) {
    case ESP_BT_STATUS_SUCCESS:
        return 0;
    case ESP_BT_STATUS_NOMEM:
        return MP_ENOMEM;
    case ESP_BT_STATUS_PARM_INVALID:
        return MP_EINVAL;
    default:
        return MP_EPERM; // fallback
    }
}

// Initialize at early boot.
void mp_bt_init(void) {
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    mp_bt_call_complete = xSemaphoreCreateBinary();
}

int mp_bt_enable(void) {
    if (mp_bt_is_enabled()) {
        mp_bt_disable();
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    err = esp_bluedroid_init();
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    err = esp_bluedroid_enable();
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    err = esp_ble_gap_register_callback(mp_bt_gap_callback);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    err = esp_ble_gatts_register_callback(mp_bt_gatts_callback);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    // Register an application profile.
    err = esp_ble_gatts_app_register(0);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    // Wait for ESP_GATTS_REG_EVT
    xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
    if (mp_bt_call_status != 0) {
        return mp_bt_status_errno();
    }
    bluetooth_gatts_if = mp_bt_call_result.gatts_if;
    return 0;
}

void mp_bt_disable(void) {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
}

bool mp_bt_is_enabled(void) {
    return esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED;
}

void mp_bt_get_addr(uint8_t *addr) {
    const uint8_t *msb = esp_bt_dev_get_address();
    // Convert from MSB to LSB.
    for (int i = 5; i >= 0; i--) {
        addr[i] = msb[5-i];
    }
}

STATIC esp_err_t mp_bt_advertise_start_internal(void) {
    esp_ble_adv_params_t ble_adv_params = {0,
        .adv_int_min       = bluetooth_adv_interval,
        .adv_int_max       = bluetooth_adv_interval,
        .adv_type          = bluetooth_adv_type,
        .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
        .channel_map       = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    return esp_ble_gap_start_advertising(&ble_adv_params);
}

int mp_bt_advertise_start(bool connectable, uint16_t interval_ms, const uint8_t *adv_data, size_t adv_data_len, const uint8_t *sr_data, size_t sr_data_len) {
    if (adv_data != NULL) {
        esp_err_t err = esp_ble_gap_config_adv_data_raw((uint8_t*)adv_data, adv_data_len);
        if (err != 0) {
            return mp_bt_esp_errno(err);
        }
        // Wait for ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT
        xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
        if (mp_bt_call_status != 0) {
            return mp_bt_status_errno();
        }
    }

    if (sr_data != NULL) {
        esp_err_t err = esp_ble_gap_config_scan_rsp_data_raw((uint8_t*)sr_data, sr_data_len);
        if (err != 0) {
            return mp_bt_esp_errno(err);
        }
        // Wait for ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT
        xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
        if (mp_bt_call_status != 0) {
            return mp_bt_status_errno();
        }
    }

    bluetooth_adv_type = connectable ? ADV_TYPE_IND : ADV_TYPE_NONCONN_IND;
    bluetooth_adv_interval = interval_ms;
    esp_err_t err = mp_bt_advertise_start_internal();
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    // Wait for ESP_GAP_BLE_ADV_START_COMPLETE_EVT
    xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
    return mp_bt_status_errno();
}

void mp_bt_advertise_stop(void) {
    esp_err_t err = esp_ble_gap_stop_advertising();
    if (err != 0) {
        return;
    }
    // Wait for ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT
    xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
}

STATIC esp_bt_uuid_t create_esp_uuid(const mp_obj_bt_uuid_t *uuid) {
    esp_bt_uuid_t result;
    if (uuid->type == MP_BT_UUID_TYPE_16) {
        result.len = ESP_UUID_LEN_16;
        result.uuid.uuid16 = uuid->uuid16;
    } else if (uuid->type == MP_BT_UUID_TYPE_32) {
        result.len = ESP_UUID_LEN_32;
        result.uuid.uuid32 = uuid->uuid32;
    } else if (uuid->type == MP_BT_UUID_TYPE_128) {
        result.len = ESP_UUID_LEN_128;
        memcpy(result.uuid.uuid128, uuid->uuid128, 16);
    }
    return result;
}

int mp_bt_add_svc(mp_obj_bt_uuid_t *svc_uuid, mp_obj_bt_uuid_t **chr_uuids, uint8_t *chr_flags, uint16_t *value_handles, size_t chr_len) {
    // Calculate the number of required handles.
    // This formula is a guess. I can't seem to find any documentation for
    // the required number of handles.
    uint16_t num_handles = 1 + chr_len * 2;
    for (size_t i = 0; i < chr_len; i++) {
        if (chr_flags[i] & MP_BT_CHR_FLAG_NOTIFY) {
            num_handles += 1;
        }
    }

    // Create the service.
    esp_gatt_srvc_id_t bluetooth_service_id;
    bluetooth_service_id.is_primary = true;
    bluetooth_service_id.id.inst_id = 0;
    bluetooth_service_id.id.uuid = create_esp_uuid(svc_uuid);
    esp_err_t err = esp_ble_gatts_create_service(bluetooth_gatts_if, &bluetooth_service_id, num_handles);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    // Wait for ESP_GATTS_CREATE_EVT
    xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
    if (mp_bt_call_status != 0) {
        return mp_bt_status_errno();
    }
    uint16_t service_handle = mp_bt_call_result.service_handle;

    // Start the service.
    err = esp_ble_gatts_start_service(service_handle);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    // Wait for ESP_GATTS_START_EVT
    xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
    if (mp_bt_call_status != 0) {
        return mp_bt_status_errno();
    }

    // Add each characteristic.
    for (size_t i = 0; i < chr_len; i++) {
        esp_gatt_perm_t perm = 0;
        perm |= (chr_flags[i] & MP_BT_CHR_FLAG_READ) ? ESP_GATT_PERM_READ : 0;
        perm |= (chr_flags[i] & MP_BT_CHR_FLAG_WRITE) ? ESP_GATT_PERM_WRITE : 0;

        esp_gatt_char_prop_t property = 0;
        property |= (chr_flags[i] & MP_BT_CHR_FLAG_READ) ? ESP_GATT_CHAR_PROP_BIT_READ : 0;
        property |= (chr_flags[i] & MP_BT_CHR_FLAG_WRITE) ? ESP_GATT_CHAR_PROP_BIT_WRITE : 0;
        property |= (chr_flags[i] & MP_BT_CHR_FLAG_NOTIFY) ? ESP_GATT_CHAR_PROP_BIT_NOTIFY : 0;

        esp_attr_value_t char_val = {0};
        char_val.attr_max_len = MP_BT_MAX_ATTR_SIZE;
        char_val.attr_len = 0;
        char_val.attr_value = NULL;

        esp_attr_control_t control = {0};
        control.auto_rsp = ESP_GATT_AUTO_RSP;

        esp_bt_uuid_t uuid = create_esp_uuid(chr_uuids[i]);

        // TODO: Who owns these pointers? (uuid, val, etc).
        esp_err_t err = esp_ble_gatts_add_char(service_handle, &uuid, perm, property, &char_val, &control);
        if (err != 0) {
            return mp_bt_esp_errno(err);
        }
        // Wait for ESP_GATTS_ADD_CHAR_EVT
        xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
        if (mp_bt_call_status != 0) {
            return mp_bt_status_errno();
        }

        // Add descriptor if needed.
        if (chr_flags[i] & MP_BT_CHR_FLAG_NOTIFY) {
            esp_attr_value_t descr_value = {0};
            descr_value.attr_max_len = 2;
            descr_value.attr_len = 2;
            // TODO: This seems wrong. Should per per-char.
            descr_value.attr_value = (uint8_t*)descr_value_buf; // looks like this buffer is never written to
            esp_err_t err = esp_ble_gatts_add_char_descr(service_handle, (esp_bt_uuid_t*)&notify_descr_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, &descr_value, &control);
            if (err != 0) {
                return mp_bt_esp_errno(err);
            }
            // Wait for ESP_GATTS_ADD_CHAR_DESCR_EVT
            xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
            if (mp_bt_call_status != 0) {
                return mp_bt_status_errno();
            }
        }

        value_handles[i] = mp_bt_call_result.attr_handle;
    }

    return 0;
}

int mp_bt_disconnect(uint16_t conn_handle) {
    esp_err_t err = esp_ble_gatts_close(bluetooth_gatts_if, conn_handle);
    return mp_bt_esp_errno(err);
}

int mp_bt_chr_value_read(uint16_t value_handle, uint8_t *value, size_t *value_len) {
    uint16_t bt_len;
    const uint8_t *bt_ptr;
    esp_err_t err = esp_ble_gatts_get_attr_value(value_handle, &bt_len, &bt_ptr);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    memcpy(value, bt_ptr, MIN(*value_len, bt_len));
    return 0;
}

int mp_bt_chr_value_write(uint16_t value_handle, const uint8_t *value, size_t *value_len) {
    esp_err_t err = esp_ble_gatts_set_attr_value(value_handle, *value_len, value);
    if (err != 0) {
        return mp_bt_esp_errno(err);
    }
    // Wait for ESP_GATTS_SET_ATTR_VAL_EVT
    xSemaphoreTake(mp_bt_call_complete, portMAX_DELAY);
    return mp_bt_status_errno();
}

int mp_bt_chr_value_notify(uint16_t conn_handle, uint16_t value_handle) {
    return mp_bt_chr_value_notify_send(conn_handle, value_handle, NULL, 0);
}

int mp_bt_chr_value_notify_send(uint16_t conn_handle, uint16_t value_handle, const uint8_t *value, size_t *value_len) {
    esp_err_t err = esp_ble_gatts_send_indicate(bluetooth_gatts_if, conn_handle, value_handle, *value_len, (void*)value, false /* notify */);
    return mp_bt_esp_errno(err);
}

int mp_bt_chr_value_indicate(uint16_t conn_handle, uint16_t value_handle) {
    esp_err_t err = esp_ble_gatts_send_indicate(bluetooth_gatts_if, conn_handle, value_handle, 0, NULL, true /* indicate */);
    return mp_bt_esp_errno(err);
}

// Event callbacks. Most API calls generate an event here to report the
// result.
STATIC void mp_bt_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            mp_bt_call_status = param->adv_data_raw_cmpl.status;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            mp_bt_call_status = param->scan_rsp_data_raw_cmpl.status;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            mp_bt_call_status = param->adv_start_cmpl.status;
            // May return an error (queue full) when called from
            // mp_bt_gatts_callback, but that's OK.
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            break;
        default:
            ESP_LOGI("bluetooth", "GAP: unknown event: %d", event);
            break;
    }
}

#define UNKNOWN_ADDR_TYPE 1

STATIC void mp_bt_gatts_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_CONNECT_EVT:
            mp_bt_central_connected(param->connect.conn_id, UNKNOWN_ADDR_TYPE, param->connect.remote_bda);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            mp_bt_central_disconnected(param->disconnect.conn_id);//, UNKNOWN_ADDR_TYPE, param->disconnect.remote_bda);
            // restart advertisement
            mp_bt_advertise_start_internal();
            break;
        case ESP_GATTS_REG_EVT:
            // Application profile created.
            mp_bt_call_status = param->reg.status;
            mp_bt_call_result.gatts_if = gatts_if;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GATTS_CREATE_EVT:
            // Service created.
            mp_bt_call_status = param->create.status;
            mp_bt_call_result.service_handle = param->create.service_handle;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GATTS_START_EVT:
            // Service started.
            mp_bt_call_status = param->start.status;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GATTS_ADD_CHAR_EVT:
            // Characteristic added.
            mp_bt_call_status = param->add_char.status;
            mp_bt_call_result.attr_handle = param->add_char.attr_handle;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            // Characteristic descriptor added.
            mp_bt_call_status = param->add_char_descr.status;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GATTS_SET_ATTR_VAL_EVT:
            // Characteristic value set by application.
            mp_bt_call_status = param->set_attr_val.status;
            xSemaphoreGive(mp_bt_call_complete);
            break;
        case ESP_GATTS_READ_EVT:
            // Characteristic value read by connected device.
            break;
        case ESP_GATTS_WRITE_EVT:
            // Characteristic value written by connected device.
            mp_bt_chr_on_write(param->write.conn_id, param->write.handle);
            // mp_bt_characteristic_on_write(param->write.conn_id, param->write.handle, param->write.value, param->write.len);
            break;
        case ESP_GATTS_CONF_EVT:
            // Characteristic notify confirmation received.
            break;
        default:
            ESP_LOGI("bluetooth", "GATTS: unknown event: %d", event);
            break;
    }
}

int mp_bt_scan_start(int32_t duration_ms) {
    return 0;
}

int mp_bt_scan_stop(void) {
    return 0;
}

int mp_bt_peripheral_connect(uint8_t addr_type, const uint8_t *addr, int32_t duration_ms) {
    return 0;
}

int mp_bt_peripheral_disc_primary_svcs(uint16_t conn_handle) {
    return 0;
}

int mp_bt_peripheral_disc_chrs(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle) {
    return 0;
}

int mp_bt_peripheral_disc_dscs(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle) {
    return 0;
}

int mp_bt_peripheral_read_chr(uint16_t conn_handle, uint16_t value_handle) {
    return 0;
}

int mp_bt_peripheral_write_chr(uint16_t conn_handle, uint16_t value_handle, const uint8_t *value, size_t *value_len) {
    return 0;
}

#endif // MICROPY_PY_BLUETOOTH
