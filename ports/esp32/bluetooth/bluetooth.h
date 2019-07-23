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

#ifndef MICROPY_INCLUDED_ESP32_BLUETOOTH_BLUETOOTH_H
#define MICROPY_INCLUDED_ESP32_BLUETOOTH_BLUETOOTH_H

void mp_bt_init(void);

#define MP_BT_CALLBACK_ALLOC            (1)
#define MP_BT_RINGBUF_SIZE              (128)
#define MP_BT_ENABLE_CENTRAL_MODE       (1)
#define MP_BT_MAX_ATTR_SIZE             (20)
#define MP_BT_INVALID_CONN_HANDLE       (0xffff)
#define MP_BT_INVALID_VALUE_HANDLE      (0xffff)

#endif // MICROPY_INCLUDED_ESP32_BLUETOOTH_BLUETOOTH_H
