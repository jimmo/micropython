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

#include "py/mphal.h"
#include "drivers/neopixel/neopixel.h"

#ifndef MP_FASTCODE
#define MP_FASTCODE(f) f
#endif

STATIC void MP_FASTCODE(neopixel_write_bitbang)(mp_hal_pin_obj_t pin, const uint32_t *timing_ticks, size_t num_pixel, const uint8_t *pixel_buf) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    const uint32_t *timing_bit;

    for (size_t i = 0; i < num_pixel; ++i) {
        uint8_t pixel_data = pixel_buf[i];
        for (size_t j = 0; j < 8; ++j) {
            mp_hal_delay_ticks_start();
            mp_hal_pin_high(pin);
            timing_bit = &timing_ticks[pixel_data >> 6 & 2];
            mp_hal_delay_ticks(timing_bit[0]);
            mp_hal_delay_ticks_start();
            mp_hal_pin_low(pin);
            pixel_data <<= 1;
            mp_hal_delay_ticks(timing_bit[1]);
        }
    }

    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

#include <stdio.h>

STATIC mp_obj_t neopixel_write(mp_obj_t pin_in, mp_obj_t timing_in, mp_obj_t buf_in) {
    // Get the pin to output to
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(pin_in);

    // Get timing values (in ns) and convert to machine-dependent loop counters.
    uint32_t timing_ticks[4];
    mp_obj_t *timing;
    mp_obj_get_array_fixed_n(timing_in, 4, &timing);
    for (size_t i = 0; i < 4; ++i) {
        uint32_t is_low_cycle = i & 1;
        timing_ticks[i] = mp_hal_delay_ticks_calc_neopixel(mp_obj_get_int(timing[i]), is_low_cycle);
        //printf("%d %d %ld\n", i, mp_obj_get_int(timing[i]), timing_ticks[i]);
    }

    // Get buffer to write
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);

    // Output the bits on the pin
    neopixel_write_bitbang(pin, timing_ticks, bufinfo.len, bufinfo.buf);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(neopixel_write_obj, neopixel_write);
