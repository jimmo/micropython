/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2014 Paul Sokolovsky
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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "py/gc.h"
#include "py/runtime.h"

#if MICROPY_ENABLE_GC

// make this 1 to dump the heap each time it changes
#define EXTENSIVE_HEAP_PROFILING (0)

// make this 1 to zero out swept memory to more eagerly
// detect untraced object still in use
#define CLEAR_ON_SWEEP (0)

// ATB = allocation table byte
// 0b00 = FREE -- free block
// 0b01 = HEAD -- head of a chain of blocks
// 0b10 = TAIL -- in the tail of a chain of blocks
// 0b11 = MARK -- marked head block

#define AT_FREE (0)
#define AT_HEAD (1)
#define AT_TAIL (2)
#define AT_MARK (3)

#define ATB_MASK_0 (0x03)
#define ATB_MASK_1 (0x0c)
#define ATB_MASK_2 (0x30)
#define ATB_MASK_3 (0xc0)

#define ATB_0_IS_FREE(a) (((a) & ATB_MASK_0) == 0)
#define ATB_1_IS_FREE(a) (((a) & ATB_MASK_1) == 0)
#define ATB_2_IS_FREE(a) (((a) & ATB_MASK_2) == 0)
#define ATB_3_IS_FREE(a) (((a) & ATB_MASK_3) == 0)

#define BLOCK_SHIFT(bl) (2 * (((bl) % GC_REGION_SIZE_BLOCKS) & (BLOCKS_PER_ATB - 1)))
#define ATB_FROM_BLOCK(bl) (((bl) % GC_REGION_SIZE_BLOCKS) / BLOCKS_PER_ATB)
#define ATB_GET_KIND(block) ((region->alloc_table[ATB_FROM_BLOCK(block)] >> BLOCK_SHIFT(block)) & 3)
#define ATB_ANY_TO_FREE(x)  do { region->alloc_table[ATB_FROM_BLOCK(x)] &= (~(AT_MARK << BLOCK_SHIFT(x))); } while (0)
#define ATB_FREE_TO_HEAD(x) do { region->alloc_table[ATB_FROM_BLOCK(x)] |= (AT_HEAD << BLOCK_SHIFT(x)); } while (0)
#define ATB_FREE_TO_TAIL(x) do { region->alloc_table[ATB_FROM_BLOCK(x)] |= (AT_TAIL << BLOCK_SHIFT(x)); } while (0)
#define ATB_HEAD_TO_MARK(x) do { region->alloc_table[ATB_FROM_BLOCK(x)] |= (AT_MARK << BLOCK_SHIFT(x)); } while (0)
#define ATB_MARK_TO_HEAD(x) do { region->alloc_table[ATB_FROM_BLOCK(x)] &= (~(AT_TAIL << BLOCK_SHIFT(x))); } while (0)

// TODO: Every time this is used we also advance the block and region, currently searching from scratch.
// This can be significantly optimised to only do the search if we overflow the current region.
#define ADVANCE_BLOCK_PTR(ptr) ptr = (void*)((uintptr_t)ptr + BYTES_PER_BLOCK)

#define PTR_FROM_BLOCK(bl) (region->addr + ((bl) % GC_REGION_SIZE_BLOCKS) * BYTES_PER_BLOCK)

#if MICROPY_PY_THREAD && !MICROPY_PY_THREAD_GIL
#define GC_ENTER() mp_thread_mutex_lock(&MP_STATE_MEM(gc_mutex), 1)
#define GC_EXIT() mp_thread_mutex_unlock(&MP_STATE_MEM(gc_mutex))
#else
#define GC_ENTER()
#define GC_EXIT()
#endif

void* port_memalign(size_t alignment, size_t n);
void port_memalign_free(void *ptr);

void gc_init(void *start, void *end) {
    printf("gc_init(" UINT_FMT ", " UINT_FMT ")\n", (uintptr_t)end, (uintptr_t)start);

    MP_STATE_MEM(gc_region_head) = NULL;

    // unlock the GC
    MP_STATE_MEM(gc_lock_depth) = 0;

    // allow auto collection
    MP_STATE_MEM(gc_auto_collect_enabled) = 1;

    MP_STATE_MEM(gc_allocated_blocks) = 0;
    MP_STATE_MEM(gc_allocated_blocks_limit) = ((uintptr_t)end - (uintptr_t)start) / BYTES_PER_BLOCK;

    #if MICROPY_GC_ALLOC_THRESHOLD
    // by default, maxuint for gc threshold, effectively turning gc-by-threshold off
    MP_STATE_MEM(gc_alloc_threshold) = (size_t)-1;
    MP_STATE_MEM(gc_alloc_amount) = 0;
    #endif

    #if MICROPY_PY_THREAD && !MICROPY_PY_THREAD_GIL
    mp_thread_mutex_init(&MP_STATE_MEM(gc_mutex));
    #endif
}

void gc_lock(void) {
    GC_ENTER();
    MP_STATE_MEM(gc_lock_depth)++;
    GC_EXIT();
}

void gc_unlock(void) {
    GC_ENTER();
    MP_STATE_MEM(gc_lock_depth)--;
    GC_EXIT();
}

bool gc_is_locked(void) {
    return MP_STATE_MEM(gc_lock_depth) != 0;
}

// Find the region and block for the specified address.
// Note: Only gc_alloc may create regions.
STATIC gc_region_t *gc_find_or_create_region(const void *addr, size_t *block, bool create) {
    *block = 0;

    // Track the last entry so we can point it to a new region if necessary.
    gc_region_t **prev = &MP_STATE_MEM(gc_region_head);

    // Current entry in the list.
    gc_region_t *region = *prev;

    // Walk the list and find a region that matches the address range.
    while (region) {
        if ((uintptr_t)addr >= region->addr && (uintptr_t)addr < region->addr + GC_REGION_SIZE_BYTES) {
            *block += ((uintptr_t)addr - region->addr) / BYTES_PER_BLOCK;
            return region;
        }

        prev = &region->next;
        region = region->next;

        *block += GC_REGION_SIZE_BLOCKS;
    }

    // Not found.
    if (!create) {
        *block = 0;
        return NULL;
    }

    // Append a new region to the list.
    region = malloc(sizeof(gc_region_t));
    if (region) {
        region->addr = (uintptr_t)addr & GC_REGION_SIZE_MASK;
        *block += ((uintptr_t)addr - region->addr) / BYTES_PER_BLOCK;
        region->next = NULL;
        memset(region->alloc_table, 0, GC_REGION_SIZE_ATBS);
        *prev = region;
    }

    return region;
}

// Check if a pointer is a known HEAD pointer, and return its block and region.
gc_region_t *is_gc_owned_head_ptr(const void *ptr, size_t *block) {
    // Fast check to see if it's even possible that this could be one of ours.
    if (((uintptr_t)(ptr) & (BYTES_PER_BLOCK - 1)) != 0) {
        // Must be aligned on a block.
        return NULL;
    }

    // This is called for all visited 32-bit words during the mark phase.
    // Might be worth a fast check to see if this is possibly in RAM area at all?

    // See if it's in a region we know about (i.e. previously returned from gc_alloc).
    gc_region_t *region = gc_find_or_create_region(ptr, block, false);
    if (!region) {
        return NULL;
    }

    // Check if the AT thinks its a HEAD (or a MARK'ed HEAD).
    if ((ATB_GET_KIND(*block) & AT_HEAD) != 0) {
        return region;
    }

    // Unknown, but definitely not a known HEAD.
    return NULL;
}

#define TRACE_MARK(block, ptr)
//printf("gc_mark(%p)\n", ptr)

// Take the given block as the topmost block on the stack. Check all it's
// children: mark the unmarked child blocks and put those newly marked
// blocks on the stack. When all children have been checked, pop off the
// topmost block on the stack and repeat with that one.
STATIC void gc_mark_subtree(void *ptr, size_t block) {
    // Start with the block passed in the argument.
    size_t sp = 0;
    for (;;) {
        // work out number of consecutive blocks in the chain starting with this one
        size_t end_block = 0;
        void *end_ptr = ptr;
        gc_region_t *region;
        do {
            ADVANCE_BLOCK_PTR(end_ptr);
            region = gc_find_or_create_region(end_ptr, &end_block, false);
        } while (region && ATB_GET_KIND(end_block) == AT_TAIL);

        size_t n_blocks = end_block - block;

        // check this block's children
        void **ptrs = (void **)ptr;
        for (size_t i = n_blocks * BYTES_PER_BLOCK / sizeof(void *); i > 0; i--, ptrs++) {
            void *block_ptr = *ptrs;
            size_t childblock;
            region = is_gc_owned_head_ptr(block_ptr, &childblock);
            if (region) {
                // Mark and push this pointer
                if (ATB_GET_KIND(childblock) == AT_HEAD) {
                    // an unmarked head, mark it, and push it on gc stack
                    TRACE_MARK(childblock, block_ptr);
                    ATB_HEAD_TO_MARK(childblock);
                    if (sp < MICROPY_ALLOC_GC_STACK_SIZE) {
                        MP_STATE_MEM(gc_stack)[sp++] = block_ptr;
                    } else {
                        MP_STATE_MEM(gc_stack_overflow) = 1;
                    }
                }
            }
        }

        // Are there any blocks on the stack?
        if (sp == 0) {
            break; // No, stack is empty, we're done.
        }

        // pop the next block off the stack
        ptr = MP_STATE_MEM(gc_stack)[--sp];
        assert(is_gc_owned_head_ptr(ptr, &block));
    }
}

STATIC void gc_deal_with_stack_overflow(void) {
    // byte *alloc_table = MP_STATE_MEM(gc_alloc_table_start);
    // while (MP_STATE_MEM(gc_stack_overflow)) {
    //     MP_STATE_MEM(gc_stack_overflow) = 0;

    //     // scan entire memory looking for blocks which have been marked but not their children
    //     for (size_t block = 0; block < MP_STATE_MEM(gc_alloc_table_byte_len) * BLOCKS_PER_ATB; block++) {
    //         // trace (again) if mark bit set
    //         if (ATB_GET_KIND(block) == AT_MARK) {
    //             gc_mark_subtree(block);
    //         }
    //     }
    // }
}

STATIC void gc_sweep(void) {
    #if MICROPY_PY_GC_COLLECT_RETVAL
    MP_STATE_MEM(gc_collected) = 0;
    #endif
    // free unmarked heads and their tails
    gc_region_t *region = MP_STATE_MEM(gc_region_head);
    size_t block = 0;
    int free_tail = 0;
    while (region) {
        for (size_t i = 0; i < GC_REGION_SIZE_BLOCKS; block++, i++) {
            switch (ATB_GET_KIND(block)) {
                case AT_HEAD:
                    free_tail = 1;
                    //printf("gc_sweep(%lu)\n", block);
                    #if MICROPY_PY_GC_COLLECT_RETVAL
                    MP_STATE_MEM(gc_collected)++;
                    #endif
                    port_memalign_free((void *)PTR_FROM_BLOCK(block));

                // fall through to free the head

                case AT_TAIL:
                    if (free_tail) {
                        ATB_ANY_TO_FREE(block);
                        #if CLEAR_ON_SWEEP
                        memset((void *)PTR_FROM_BLOCK(block), 0, BYTES_PER_BLOCK);
                        #endif
                        MP_STATE_MEM(gc_allocated_blocks) -= 1;
                    }
                    break;

                case AT_MARK:
                    ATB_MARK_TO_HEAD(block);
                    free_tail = 0;
                    break;
            }
        }
        region = region->next;
    }
}

void gc_collect_start(void) {
    GC_ENTER();
    MP_STATE_MEM(gc_lock_depth)++;
    #if MICROPY_GC_ALLOC_THRESHOLD
    MP_STATE_MEM(gc_alloc_amount) = 0;
    #endif
    MP_STATE_MEM(gc_stack_overflow) = 0;

    // Trace root pointers.  This relies on the root pointers being organised
    // correctly in the mp_state_ctx structure.  We scan nlr_top, dict_locals,
    // dict_globals, then the root pointer section of mp_state_vm.
    void **ptrs = (void **)(void *)&mp_state_ctx;
    size_t root_start = offsetof(mp_state_ctx_t, thread.dict_locals);
    size_t root_end = offsetof(mp_state_ctx_t, vm.qstr_last_chunk);
    gc_collect_root(ptrs + root_start / sizeof(void *), (root_end - root_start) / sizeof(void *));

    #if MICROPY_ENABLE_PYSTACK
    // Trace root pointers from the Python stack.
    ptrs = (void **)(void *)MP_STATE_THREAD(pystack_start);
    gc_collect_root(ptrs, (MP_STATE_THREAD(pystack_cur) - MP_STATE_THREAD(pystack_start)) / sizeof(void *));
    #endif
}

void gc_collect_root(void **ptrs, size_t len) {
    for (size_t i = 0; i < len; i++) {
        void *ptr = ptrs[i];
        size_t block;
        gc_region_t *region = is_gc_owned_head_ptr(ptr, &block);
        if (region) {
            // An unmarked head: mark it, and mark all its children
            TRACE_MARK(block, ptr);
            ATB_HEAD_TO_MARK(block);
            gc_mark_subtree(ptr, block);
        }
    }
}

void gc_collect_end(void) {
    gc_deal_with_stack_overflow();
    gc_sweep();
    MP_STATE_MEM(gc_last_free_atb_index) = 0;
    MP_STATE_MEM(gc_lock_depth)--;
    GC_EXIT();
}

void gc_sweep_all(void) {
    GC_ENTER();
    MP_STATE_MEM(gc_lock_depth)++;
    MP_STATE_MEM(gc_stack_overflow) = 0;
    gc_collect_end();
}

void gc_info(gc_info_t *info) {
    GC_ENTER();
    info->total = MP_STATE_MEM(gc_allocated_blocks_limit) * BYTES_PER_BLOCK;
    info->used = MP_STATE_MEM(gc_allocated_blocks) * BYTES_PER_BLOCK;
    info->free = info->total - info->used;
    info->max_free = 0;
    info->num_1block = 0;
    info->num_2block = 0;
    info->max_block = 0;
    GC_EXIT();
    // info->max_free = 0;
    // info->num_1block = 0;
    // info->num_2block = 0;
    // info->max_block = 0;
    // bool finish = false;
    // for (size_t block = 0, len = 0, len_free = 0; !finish;) {
    //     size_t kind = ATB_GET_KIND(block);
    //     switch (kind) {
    //         case AT_FREE:
    //             info->free += 1;
    //             len_free += 1;
    //             len = 0;
    //             break;

    //         case AT_HEAD:
    //             info->used += 1;
    //             len = 1;
    //             break;

    //         case AT_TAIL:
    //             info->used += 1;
    //             len += 1;
    //             break;

    //         case AT_MARK:
    //             // shouldn't happen
    //             break;
    //     }

    //     block++;
    //     finish = (block == MP_STATE_MEM(gc_alloc_table_byte_len) * BLOCKS_PER_ATB);
    //     // Get next block type if possible
    //     if (!finish) {
    //         kind = ATB_GET_KIND(block);
    //     }

    //     if (finish || kind == AT_FREE || kind == AT_HEAD) {
    //         if (len == 1) {
    //             info->num_1block += 1;
    //         } else if (len == 2) {
    //             info->num_2block += 1;
    //         }
    //         if (len > info->max_block) {
    //             info->max_block = len;
    //         }
    //         if (finish || kind == AT_HEAD) {
    //             if (len_free > info->max_free) {
    //                 info->max_free = len_free;
    //             }
    //             len_free = 0;
    //         }
    //     }
    // }

    // info->used *= BYTES_PER_BLOCK;
    // info->free *= BYTES_PER_BLOCK;
    // GC_EXIT();
}

void *gc_alloc(size_t n_bytes, unsigned int alloc_flags) {
    size_t n_blocks = ((n_bytes + BYTES_PER_BLOCK - 1) & (~(BYTES_PER_BLOCK - 1))) / BYTES_PER_BLOCK;
    //printf("gc_alloc(" UINT_FMT " bytes -> " UINT_FMT " blocks), available " UINT_FMT "\n", n_bytes, n_blocks, MP_STATE_MEM(gc_allocated_blocks_limit) - MP_STATE_MEM(gc_allocated_blocks));

    // check for 0 allocation
    if (n_blocks == 0) {
        return NULL;
    }


    GC_ENTER();

    // check if GC is locked
    if (MP_STATE_MEM(gc_lock_depth) > 0) {
        GC_EXIT();
        return NULL;
    }

    void* ret_ptr = NULL;

    bool collected = false;
    while (true) {
        if (n_blocks + MP_STATE_MEM(gc_allocated_blocks) < MP_STATE_MEM(gc_allocated_blocks_limit)) {
            ret_ptr = port_memalign(BYTES_PER_BLOCK, n_blocks * BYTES_PER_BLOCK);
        }
        if (ret_ptr) {
            break;
        }

        GC_EXIT();
        if (collected) {
            return NULL;
        }
        gc_collect();
        collected = true;
        GC_ENTER();
    }

    size_t block;
    gc_region_t *region = gc_find_or_create_region(ret_ptr, &block, true);
    assert(region);
    if (!region) {
        // Out of memory allocating region bitmap.
        port_memalign_free(ret_ptr);
        GC_EXIT();
        return NULL;
    }

    // mark first block as used head
    assert(ATB_GET_KIND(block) == AT_FREE);
    ATB_FREE_TO_HEAD(block);

    void *block_ptr = ret_ptr;

    // mark rest of blocks as used tail
    // TODO for a run of many blocks can make this more efficient
    for (size_t i = 1; i < n_blocks; ++i) {
        ADVANCE_BLOCK_PTR(block_ptr);
        region = gc_find_or_create_region(block_ptr, &block, true);
        assert(region);
        if (!region) {
            // Out of memory allocating region bitmap.
            port_memalign_free(ret_ptr);
            GC_EXIT();
            return NULL;
        }
        ATB_FREE_TO_TAIL(block);
    }

    #if MICROPY_GC_ALLOC_THRESHOLD
    MP_STATE_MEM(gc_alloc_amount) += n_blocks;
    #endif

    MP_STATE_MEM(gc_allocated_blocks) += n_blocks;

    GC_EXIT();

    #if MICROPY_GC_CONSERVATIVE_CLEAR
    // be conservative and zero out all the newly allocated blocks
    memset((byte *)ret_ptr, 0, n_blocks * BYTES_PER_BLOCK);
    #else
    // zero out the additional bytes of the newly allocated blocks
    // This is needed because the blocks may have previously held pointers
    // to the heap and will not be set to something else if the caller
    // doesn't actually use the entire block.  As such they will continue
    // to point to the heap and may prevent other blocks from being reclaimed.
    memset((byte *)ret_ptr + n_bytes, 0, (end_block - start_block + 1) * BYTES_PER_BLOCK - n_bytes);
    #endif

    #if EXTENSIVE_HEAP_PROFILING
    gc_dump_alloc_table();
    #endif

    return ret_ptr;
}

// force the freeing of a piece of memory
// TODO: freeing here does not call finaliser
void gc_free(void *ptr) {
    GC_ENTER();
    if (MP_STATE_MEM(gc_lock_depth) > 0) {
        // TODO how to deal with this error?
        GC_EXIT();
        return;
    }

    if (ptr == NULL) {
        GC_EXIT();
    } else {
        // Get the GC block number corresponding to this pointer.
        size_t block;
        gc_region_t *region = is_gc_owned_head_ptr(ptr, &block);
        assert(region);
        port_memalign_free(ptr);

        // Free head and all of its tail blocks.
        do {
            ATB_ANY_TO_FREE(block);
            MP_STATE_MEM(gc_allocated_blocks) -= 1;
            ADVANCE_BLOCK_PTR(ptr);
            region = gc_find_or_create_region(ptr, &block, false);
        } while (region && ATB_GET_KIND(block) == AT_TAIL);

        GC_EXIT();

        #if EXTENSIVE_HEAP_PROFILING
        gc_dump_alloc_table();
        #endif
    }
}

size_t gc_nbytes(const void *ptr) {
    if (!ptr) {
        return 0;
    }
    GC_ENTER();
    size_t block;
    gc_region_t *region = is_gc_owned_head_ptr(ptr, &block);
    assert(region);
    if (region) {
        // work out number of consecutive blocks in the chain starting with this on
        size_t n_bytes = 0;
        do {
            n_bytes += BYTES_PER_BLOCK;
            ADVANCE_BLOCK_PTR(ptr);
            region = gc_find_or_create_region(ptr, &block, false);
        } while (region && ATB_GET_KIND(block) == AT_TAIL);
        GC_EXIT();
        return n_bytes;
    }

    // invalid pointer
    GC_EXIT();
    return 0;
}

// old, simple realloc that didn't expand memory in place
void *gc_realloc(void *ptr, size_t n_bytes, bool allow_move) {
    //printf("gc_realloc(%p, " UINT_FMT ")\n", ptr, n_bytes);
    mp_uint_t n_existing = gc_nbytes(ptr);
    if (n_bytes <= n_existing) {
        return ptr;
    } else {
        if (!allow_move) {
            return NULL;
        }
        void *ptr2 = gc_alloc(n_bytes, 0);
        if (ptr2 == NULL) {
            return ptr2;
        }
        memcpy(ptr2, ptr, n_existing);
        gc_free(ptr);
        return ptr2;
    }
}

void gc_dump_info(void) {
    gc_info_t info;
    gc_info(&info);
    mp_printf(&mp_plat_print, "GC: total: %u, used: %u, free: %u\n",
        (uint)info.total, (uint)info.used, (uint)info.free);
    mp_printf(&mp_plat_print, " No. of 1-blocks: %u, 2-blocks: %u, max blk sz: %u, max free sz: %u\n",
        (uint)info.num_1block, (uint)info.num_2block, (uint)info.max_block, (uint)info.max_free);
}

void gc_dump_alloc_table(void) {
    GC_ENTER();
    static const size_t DUMP_BYTES_PER_LINE = 64;
    #if !EXTENSIVE_HEAP_PROFILING
    // When comparing heap output we don't want to print the starting
    // pointer of the heap because it changes from run to run.
    mp_printf(&mp_plat_print, "GC memory layout\n");
    #endif

    gc_region_t *region = MP_STATE_MEM(gc_region_head);
    size_t bl = 0;
    while (region) {
        mp_printf(&mp_plat_print, "Region at 0x%p, starting with block 0x%x\n", region->addr, bl);
        for (size_t i = 0; i < GC_REGION_SIZE_BLOCKS; bl++, i++) {
            if (bl % DUMP_BYTES_PER_LINE == 0) {
                // print header for new line of blocks
                // (the cast to uint32_t is for 16-bit ports)
                //mp_printf(&mp_plat_print, "\n%05x: ", (uint)(PTR_FROM_BLOCK(bl) & (uint32_t)0xfffff));
                mp_printf(&mp_plat_print, "\n%05x: ", i);
            }
            int c = ' ';
            switch (ATB_GET_KIND(bl)) {
                case AT_FREE:
                    c = '.';
                    break;

                /* this prints the uPy object type of the head block */
                case AT_HEAD: {
                    void **ptr = (void **)(region->addr + i * BYTES_PER_BLOCK);
                    if (*ptr == &mp_type_tuple) {
                        c = 'T';
                    } else if (*ptr == &mp_type_list) {
                        c = 'L';
                    } else if (*ptr == &mp_type_dict) {
                        c = 'D';
                    } else if (*ptr == &mp_type_str || *ptr == &mp_type_bytes) {
                        c = 'S';
                    }
                    #if MICROPY_PY_BUILTINS_BYTEARRAY
                    else if (*ptr == &mp_type_bytearray) {
                        c = 'A';
                    }
                    #endif
                    #if MICROPY_PY_ARRAY
                    else if (*ptr == &mp_type_array) {
                        c = 'A';
                    }
                    #endif
                    #if MICROPY_PY_BUILTINS_FLOAT
                    else if (*ptr == &mp_type_float) {
                        c = 'F';
                    }
                    #endif
                    else if (*ptr == &mp_type_fun_bc) {
                        c = 'B';
                    } else if (*ptr == &mp_type_module) {
                        c = 'M';
                    } else {
                        c = 'h';
                        #if 0
                        // This code prints "Q" for qstr-pool data, and "q" for qstr-str
                        // data.  It can be useful to see how qstrs are being allocated,
                        // but is disabled by default because it is very slow.
                        for (qstr_pool_t *pool = MP_STATE_VM(last_pool); c == 'h' && pool != NULL; pool = pool->prev) {
                            if ((qstr_pool_t *)ptr == pool) {
                                c = 'Q';
                                break;
                            }
                            for (const byte **q = pool->qstrs, **q_top = pool->qstrs + pool->len; q < q_top; q++) {
                                if ((const byte *)ptr == *q) {
                                    c = 'q';
                                    break;
                                }
                            }
                        }
                        #endif
                    }
                    break;
                }
                case AT_TAIL:
                    c = '=';
                    break;
                case AT_MARK:
                    c = 'm';
                    break;
            }
            mp_printf(&mp_plat_print, "%c", c);
        }

        mp_print_str(&mp_plat_print, "\n");

        region = region->next;
    }
    GC_EXIT();
}

#if 0
// For testing the GC functions
void gc_test(void) {
    mp_uint_t len = 500;
    mp_uint_t *heap = malloc(len);
    gc_init(heap, heap + len / sizeof(mp_uint_t));
    void *ptrs[100];
    {
        mp_uint_t **p = gc_alloc(16, false);
        p[0] = gc_alloc(64, false);
        p[1] = gc_alloc(1, false);
        p[2] = gc_alloc(1, false);
        p[3] = gc_alloc(1, false);
        mp_uint_t ***p2 = gc_alloc(16, false);
        p2[0] = p;
        p2[1] = p;
        ptrs[0] = p2;
    }
    for (int i = 0; i < 25; i += 2) {
        mp_uint_t *p = gc_alloc(i, false);
        printf("p=%p\n", p);
        if (i & 3) {
            //ptrs[i] = p;
        }
    }

    printf("Before GC:\n");
    gc_dump_alloc_table();
    printf("Starting GC...\n");
    gc_collect_start();
    gc_collect_root(ptrs, sizeof(ptrs) / sizeof(void *));
    gc_collect_end();
    printf("After GC:\n");
    gc_dump_alloc_table();
}
#endif

#endif // MICROPY_ENABLE_GC
