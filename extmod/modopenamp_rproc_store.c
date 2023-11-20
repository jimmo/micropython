/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2024 Arduino SA
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
 *
 * OpenAMP's remoteproc store.
 */
#if MICROPY_PY_OPENAMP_RPROC_STORE_ENABLE
#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "extmod/vfs.h"

#include "metal/alloc.h"
#include "metal/errno.h"
#include "metal/io.h"
#include "openamp/remoteproc.h"
#include "openamp/remoteproc_loader.h"
#include "modopenamp_rproc.h"

#define DEBUG_printf(...)   // mp_printf(&mp_plat_print, __VA_ARGS__)

// Note the initial file buffer size needs to be at least 512 to read
// enough of the elf headers on the first call to store_open(), and on
// subsequent calls to store functions, it gets reallocated if needed.
#define RPROC_FILE_STORE_BUF_SIZE   (1024)

typedef struct rproc_filestore {
    size_t len;
    uint8_t *buf;
    mp_obj_t file;
} rproc_filestore_t;

void *mp_openamp_rproc_store_alloc(void) {
    // Allocate an rproc filestore.
    rproc_filestore_t *fstore;
    fstore = metal_allocate_memory(sizeof(rproc_filestore_t));
    fstore->len = RPROC_FILE_STORE_BUF_SIZE;
    fstore->buf = metal_allocate_memory(RPROC_FILE_STORE_BUF_SIZE);
    return fstore;
}

STATIC int store_open(void *store, const char *path, const void **image_data) {
    DEBUG_printf("store_open(): %s\n", path);
    mp_obj_t args[2] = {
        mp_obj_new_str(path, strlen(path)),
        MP_OBJ_NEW_QSTR(MP_QSTR_rb),
    };

    rproc_filestore_t *fstore = store;
    fstore->file = mp_vfs_open(MP_ARRAY_SIZE(args), args, (mp_map_t *)&mp_const_empty_map);

    int error = 0;
    mp_uint_t bytes = mp_stream_read_exactly(fstore->file, fstore->buf, RPROC_FILE_STORE_BUF_SIZE, &error);
    if (error != 0 || bytes != RPROC_FILE_STORE_BUF_SIZE) {
        return -EINVAL;
    }
    *image_data = fstore->buf;
    return bytes;
}

STATIC void store_close(void *store) {
    DEBUG_printf("store_close()\n");
    rproc_filestore_t *fstore = store;
    mp_stream_close(fstore->file);
    metal_free_memory(fstore->buf);
    metal_free_memory(fstore);
}

STATIC int store_load(void *store, size_t offset, size_t size,
    const void **data, metal_phys_addr_t pa,
    struct metal_io_region *io,
    char is_blocking) {

    int error = 0;
    rproc_filestore_t *fstore = store;

    if (mp_stream_seek(fstore->file, offset, MP_SEEK_SET, &error) == -1) {
        return -EINVAL;
    }

    if (pa == METAL_BAD_PHYS) {
        if (size > fstore->len) {
            // Note tracked allocs don't support realloc.
            fstore->len = size;
            fstore->buf = metal_allocate_memory(size);
            DEBUG_printf("store_load() realloc to %lu\n", fstore->len);
        }
        *data = fstore->buf;
        DEBUG_printf("store_load(): pa 0x%lx offset %u size %u \n", (uint32_t)pa, offset, size);
    } else {
        void *va = metal_io_phys_to_virt(io, pa);
        if (va == NULL) {
            return -EINVAL;
        }
        *data = va;
        DEBUG_printf("store_load(): pa 0x%lx va 0x%p offset %u size %u \n", (uint32_t)pa, va, offset, size);
    }

    mp_uint_t bytes = mp_stream_read_exactly(fstore->file, (void *)*data, size, &error);
    if (bytes != size || error != 0) {
        return -EINVAL;
    }

    return bytes;
}

const struct image_store_ops mp_openamp_rproc_store_ops = {
    .open = store_open,
    .close = store_close,
    .load = store_load,
    .features = SUPPORT_SEEK,
};
#endif // MICROPY_PY_OPENAMP_RPROC_STORE_ENABLE
