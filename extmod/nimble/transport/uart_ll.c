/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
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

/*
 * Parts of this file based on nimble/transport/uart_ll/src/hci_uart.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

// This provides a LL transport for the NimBLE Host. It's the same as NimBLE's
// uart_ll, except without the callback mechanism to request byte-at-a-time,
// and also provides locking to avoid concurrent attempts to send CMD or ACL
// payloads.

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "os/os_mbuf.h"
#include "os/os_mempool.h"
#include "nimble/transport.h"
#include "nimble/transport/hci_h4.h"

#include "extmod/nimble/modbluetooth_nimble.h"
#include "extmod/nimble/nimble/nimble_npl_os.h"
#include "extmod/mpbthci.h"
#include "py/mpthread.h"

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_NIMBLE

#ifndef MICROPY_PY_BLUETOOTH_HCI_READ_MODE
#define MICROPY_PY_BLUETOOTH_HCI_READ_MODE MICROPY_PY_BLUETOOTH_HCI_READ_MODE_BYTE
#endif

#define HCI_TRACE (0)
#define COL_OFF "\033[0m"
#define COL_GREEN "\033[0;32m"
#define COL_BLUE "\033[0;34m"

// Provided by the port, and also possibly shared with the driver.
extern uint8_t mp_bluetooth_hci_cmd_buf[4 + 256];

STATIC struct hci_h4_sm hci_uart_h4sm;

// NimBLE does not protect an ACL and a CMD packet from being sent to the
// transport at the same time. mp_bluetooth_hci_uart_write assumes that it
// won't be called concurrently.
STATIC mp_thread_mutex_t ble_hs_uart_hci_mutex;

STATIC int hci_uart_frame_cb(uint8_t pkt_type, void *data) {
    switch (pkt_type) {
    case HCI_H4_EVT:
        return ble_transport_to_hs_evt(data);
    case HCI_H4_ACL:
        return ble_transport_to_hs_acl(data);
    default:
        assert(0);
        break;
    }

    return -1;
}

STATIC void mp_bluetooth_hci_uart_char_cb(uint8_t chr) {
    #if HCI_TRACE
    printf(COL_BLUE "> [% 8d] %02x" COL_OFF "\n", (int)mp_hal_ticks_ms(), chr);
    #endif
    hci_h4_sm_rx(&hci_uart_h4sm, &chr, 1);
}

extern struct ble_npl_eventq* g_eventq_dflt;

bool mp_bluetooth_nimble_hci_uart_process(void) {
    bool host_wake = mp_bluetooth_hci_controller_woken();

    for (;;) {
        #if MICROPY_PY_BLUETOOTH_HCI_READ_MODE == MICROPY_PY_BLUETOOTH_HCI_READ_MODE_BYTE
        int chr = mp_bluetooth_hci_uart_readchar();
        if (chr < 0) {
            break;
        }
        mp_bluetooth_hci_uart_char_cb(chr);
        #elif MICROPY_PY_BLUETOOTH_HCI_READ_MODE == MICROPY_PY_BLUETOOTH_HCI_READ_MODE_PACKET
        if (mp_bluetooth_hci_uart_readpacket(mp_bluetooth_hci_uart_char_cb) < 0) {
            break;
        }
        #endif

        os_sr_t sr;
        OS_ENTER_CRITICAL(sr);
        struct ble_npl_event *ev = g_eventq_dflt->head;
        OS_EXIT_CRITICAL(sr);
        if (ev) {
            // Prioritise running the event queue over handling more UART data.
            return true;
        }
    }

    if (host_wake) {
        mp_bluetooth_hci_controller_sleep_maybe();
    }

    return false;
}

#if HCI_TRACE
STATIC void trace_cmd_buf(size_t len) {
    printf(COL_GREEN "< [% 8d] %02x", (int)mp_hal_ticks_ms(), mp_bluetooth_hci_cmd_buf[0]);
    for (size_t i = 1; i < len; ++i) {
        printf(":%02x", mp_bluetooth_hci_cmd_buf[i]);
    }
    printf(COL_OFF "\n");
}
#else
STATIC void trace_cmd_buf(size_t len) {
}
#endif

int ble_transport_to_ll_cmd_impl(void *buf_in) {
    uint8_t *buf = buf_in;
    size_t len = 3 + buf[2];

    mp_thread_mutex_lock(&ble_hs_uart_hci_mutex, true);
    mp_bluetooth_hci_cmd_buf[0] = HCI_H4_CMD;
    memcpy(&mp_bluetooth_hci_cmd_buf[1], buf, len);
    trace_cmd_buf(len + 1);
    mp_bluetooth_hci_uart_write(mp_bluetooth_hci_cmd_buf, len + 1);
    mp_thread_mutex_unlock(&ble_hs_uart_hci_mutex);

    ble_transport_free(buf);

    // TODO: This is probably only necessary for ACL?
    if (len > 0) {
        // Allow modbluetooth bindings to hook "sent packet" (e.g. to un-stall
        // l2cap channels).
        mp_bluetooth_nimble_sent_hci_packet();
    }

    return 0;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
    mp_bluetooth_hci_cmd_buf[0] = HCI_H4_ACL;
    size_t len = OS_MBUF_PKTLEN(om);

    mp_thread_mutex_lock(&ble_hs_uart_hci_mutex, true);
    os_mbuf_copydata(om, 0, len, &mp_bluetooth_hci_cmd_buf[1]);
    trace_cmd_buf(len + 1);
    mp_bluetooth_hci_uart_write(mp_bluetooth_hci_cmd_buf, len + 1);
    mp_thread_mutex_unlock(&ble_hs_uart_hci_mutex);

    os_mbuf_free_chain(om);

    if (len > 0) {
        // Allow modbluetooth bindings to hook "sent packet" (e.g. to un-stall
        // l2cap channels).
        mp_bluetooth_nimble_sent_hci_packet();
    }

    return 0;
}

void ble_transport_ll_init(void) {
    SYSINIT_ASSERT_ACTIVE();

    mp_thread_mutex_init(&ble_hs_uart_hci_mutex);

    int rc = mp_bluetooth_hci_uart_init(MYNEWT_VAL(BLE_TRANSPORT_UART_PORT), MYNEWT_VAL(BLE_TRANSPORT_UART_BAUDRATE));
    (void)rc;
    SYSINIT_PANIC_ASSERT(rc == 0);

    hci_h4_sm_init(&hci_uart_h4sm, &hci_h4_allocs_from_ll, hci_uart_frame_cb);
}

#endif // MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_NIMBLE
