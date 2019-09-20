/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
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

#include "py/mpconfig.h"

// All the qstr definitions in this file are available as constants.
// That is, they are in ROM and you can reference them simply as MP_QSTR_xxxx.

// qstr configuration passed to makeqstrdata.py of the form QCFG(key, value)
QCFG(BYTES_IN_LEN, MICROPY_QSTR_BYTES_IN_LEN)
QCFG(BYTES_IN_HASH, MICROPY_QSTR_BYTES_IN_HASH)

Q()
Q(*)
Q(_)
Q(/)
#if MICROPY_PY_BUILTINS_STR_OP_MODULO
Q(%#o)
Q(%#x)
#else
Q({:#o})
Q({:#x})
#endif
Q({:#b})
Q( )
Q(\n)
Q(maximum recursion depth exceeded)
Q(<module>)
Q(<lambda>)
Q(<listcomp>)
Q(<dictcomp>)
Q(<setcomp>)
Q(<genexpr>)
Q(<string>)
Q(<stdin>)
Q(utf-8)
#if MICROPY_HW_HAS_F7DLCD
Q(clear)
Q(clear_string_line)
Q(deinit)
Q(display_char)
Q(display_off)
Q(display_on)
Q(display_string_at)
Q(display_string_at_line)
Q(draw_Hline)
Q(draw_Vline)
Q(draw_bitmap)
Q(draw_circle)
Q(draw_ellipse)
Q(draw_line)
Q(draw_pixel)
Q(draw_polygon)
Q(draw_rect)
Q(fill_circle)
Q(fill_ellipse)
Q(fill_polygon)
Q(fill_rect)
Q(font_height)
Q(font_width)
//Q(fill_triangle)
Q(get_back_color)
Q(get_font)
Q(get_size)
Q(get_text_color)
Q(get_x_size)
Q(get_y_size)
Q(init)
Q(layer_default_init)
Q(layer_rgb565_init)
Q(lcdF7D)
Q(read_pixel)
Q(reload)
Q(reset_color_keying)
Q(reset_color_keying_noreload)
Q(scroll)
Q(select_layer)
Q(set_back_color)
Q(set_color_keying)
Q(set_color_keying_noreload)
Q(set_font)
Q(set_layer_visible_noreload)
Q(set_layer_address)
Q(set_layer_address_noreload)
Q(set_layer_visible)
Q(set_layer_window)
Q(set_layer_window_noreload)
Q(set_size)
Q(set_text_color)
Q(set_transparency)
Q(set_transparency_noreload)
Q(set_x_size)
Q(set_y_size)
#endif  // MICROPY_HW_HAS_F7DLCD
#if MICROPY_HW_HAS_F7DTCH
Q(deinit)
Q(gesture)
Q(get_gesture_id)
Q(get_state)
Q(init)
Q(point_info)
Q(reset_touch_data)
Q(tchF7D)
Q(touches)
#endif  //  MICROPY_HW_HAS_F7DTCH
#if MICROPY_ENABLE_PYSTACK
Q(pystack exhausted)
#endif
