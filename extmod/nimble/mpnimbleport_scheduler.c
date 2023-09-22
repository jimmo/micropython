/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Jim Mussared
 * Copyright (c) 2020 Damien P. George
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

// This is a standard implementation of mpnimbleport.c for ports that run
// NimBLE entirely in the scheduler (STM32, etc).

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_NIMBLE

#define DEBUG_printf(...) // printf("mpnimbleport.c: " __VA_ARGS__)

#include "nimble/nimble_npl.h"

#include "extmod/nimble/modbluetooth_nimble.h"
#include "extmod/nimble/transport/uart_ll.h"

// Port-specific header for mp_bluetooth_hci_poll_in_ms.
#include "mpbthciport.h"

bool mp_bluetooth_run_hci_uart(void) {
    if (mp_bluetooth_nimble_ble_state >= MP_BLUETOOTH_NIMBLE_BLE_STATE_WAITING_FOR_SYNC) {
        // Get the LL transport to process any incoming UART data.
        mp_bluetooth_nimble_hci_uart_process();
    }

    return true;
}

bool mp_bluetooth_run_host_stack(void) {
    if (mp_bluetooth_nimble_ble_state >= MP_BLUETOOTH_NIMBLE_BLE_STATE_WAITING_FOR_SYNC) {
        // Run any timers and pending events in the queue.
        mp_bluetooth_nimble_run_host_stack();
    }

    if (mp_bluetooth_nimble_ble_state != MP_BLUETOOTH_NIMBLE_BLE_STATE_OFF) {
        // Call this function again in 128ms to check for new events.
        // TODO: improve this by only calling back when needed.
        mp_bluetooth_hci_poll_in_ms(128);
    }

    return true;
}

// --- Port-specific helpers for the generic NimBLE bindings. -----------------

void mp_bluetooth_nimble_hci_uart_wfi(void) {
    __WFI();

    // This is called while NimBLE is waiting in ble_npl_sem_pend, i.e.
    // waiting for an HCI ACK. Do not need to run events here (it must not
    // invoke Python code), only processing incoming HCI data. Ideally what
    // should happen instead is that the UART IRQ should call
    // mp_bluetooth_run_hci_uart directly, letting the UART handling run in
    // IRQ context.
    mp_bluetooth_nimble_hci_uart_process();

}

#endif // MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_NIMBLE
