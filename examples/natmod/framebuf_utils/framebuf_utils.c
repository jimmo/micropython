#define MICROPY_PY_FRAMEBUF (1)

#include "py/dynruntime.h"

#if !defined(__linux__)
void *memset(void *s, int c, size_t n) {
    return mp_fun_table.memset_(s, c, n);
}
#endif

// Match definition from modframebuf.c.
typedef struct _mp_obj_framebuf_t {
    mp_obj_base_t base;
    mp_obj_t buf_obj; // need to store this to prevent GC from reclaiming buf
    void *buf;
    uint16_t width, height, stride;
    uint8_t format;
} mp_obj_framebuf_t;

// Simple version of mp_map_lookup (expects fixed & ordered map).
STATIC mp_obj_t mp_map_find(mp_map_t *map, mp_obj_t index) {
    for (size_t i = 0; i < map->used; ++i) {
        if (map->table[i].key == index) {
            return map->table[i].value;
        }
    }
    mp_raise_ValueError(NULL);
}

// render(FrameBuffer, dest, src, x, y, fgcolor, bgcolor=0)
STATIC mp_obj_t framebuf_render(size_t n_args, const mp_obj_t *args) {
    // This is equivalent to &mp_type_framebuf.
    mp_obj_type_t *framebuf_type = MP_OBJ_TO_PTR(args[0]);

    // Get the "pixel" method from the FrameBuffer type.
    mp_obj_t framebuf_pixel_obj = mp_map_find(&framebuf_type->locals_dict->map, MP_OBJ_NEW_QSTR(MP_QSTR_pixel));
    mp_obj_fun_builtin_var_t *var_fun = MP_OBJ_TO_PTR(framebuf_pixel_obj);
    mp_fun_var_t framebuf_pixel = var_fun->fun.var;

    // Convert dest/src subclass to the native mp_type_framebuf.
    mp_obj_t dest_in = mp_obj_cast_to_native_base(args[1], MP_OBJ_FROM_PTR(framebuf_type));
    if (dest_in == MP_OBJ_NULL) {
        mp_raise_TypeError(NULL);
    }
    mp_obj_framebuf_t *dest = MP_OBJ_TO_PTR(dest_in);

    mp_obj_t source_in = mp_obj_cast_to_native_base(args[2], MP_OBJ_FROM_PTR(framebuf_type));
    if (source_in == MP_OBJ_NULL) {
        mp_raise_TypeError(NULL);
    }
    mp_obj_framebuf_t *source = MP_OBJ_TO_PTR(source_in);

    // Pre-build args list for calling framebuf.pixel().
    mp_obj_t args_getpixel[3] = { source_in };
    mp_obj_t args_setpixel[4] = { dest_in };

    mp_int_t x = mp_obj_get_int(args[3]);
    mp_int_t y = mp_obj_get_int(args[4]);
    mp_int_t fgcol = mp_obj_get_int(args[5]);
    mp_int_t bgcol = 0;
    if (n_args > 6) {
        bgcol = mp_obj_get_int(args[6]);
    }

    if (
        (x >= dest->width) ||
        (y >= dest->height) ||
        (-x >= source->width) ||
        (-y >= source->height)
        ) {
        // Out of bounds, no-op.
        return mp_const_none;
    }

    // Clip.
    int x0 = MAX(0, x);
    int y0 = MAX(0, y);
    int x1 = MAX(0, -x);
    int y1 = MAX(0, -y);
    int x0end = MIN(dest->width, x + source->width);
    int y0end = MIN(dest->height, y + source->height);

    for (; y0 < y0end; ++y0) {
        int cx1 = x1;
        for (int cx0 = x0; cx0 < x0end; ++cx0) {
            // source.pixel(cx1, y1)
            args_getpixel[1] = MP_OBJ_NEW_SMALL_INT(cx1);
            args_getpixel[2] = MP_OBJ_NEW_SMALL_INT(y1);
            uint32_t col = mp_obj_get_int(framebuf_pixel(3, args_getpixel));

            // dest.pixel(cx0, y0, bgcol/fgcol)
            args_setpixel[1] = MP_OBJ_NEW_SMALL_INT(cx0);
            args_setpixel[2] = MP_OBJ_NEW_SMALL_INT(y0);
            if (col == 0) {
                args_setpixel[3] = MP_OBJ_NEW_SMALL_INT(bgcol);
            } else {
                args_setpixel[3] = MP_OBJ_NEW_SMALL_INT(fgcol);
            }

            framebuf_pixel(4, args_setpixel);

            ++cx1;
        }
        ++y1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(framebuf_render_obj, 6, 7, framebuf_render);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY

    mp_store_global(MP_QSTR_render, MP_OBJ_FROM_PTR(&framebuf_render_obj));

    MP_DYNRUNTIME_INIT_EXIT
}
