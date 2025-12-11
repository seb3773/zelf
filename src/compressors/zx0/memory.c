/*
 * (c) Copyright 2021 by Einar Saukas. All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include "zx0_internal.h"

/* Increase pre-allocation to reduce malloc() overhead */
#define QTY_BLOCKS 100000

static BLOCK *ghost_root = NULL;
static BLOCK *dead_array = NULL;
static int dead_array_size = 0;

__attribute__((always_inline)) inline BLOCK *zx0_allocate(int bits, int index, int offset, BLOCK *chain) {
    BLOCK *ptr;

    if (ghost_root) {
        ptr = ghost_root;
        ghost_root = ptr->ghost_chain;
        if (ptr->chain && !--ptr->chain->references) {
            ptr->chain->ghost_chain = ghost_root;
            ghost_root = ptr->chain;
        }
    } else {
        if (!dead_array_size) {
            dead_array = (BLOCK *)malloc((size_t)QTY_BLOCKS*sizeof(BLOCK));
            if (!dead_array) {
                /* Error: Insufficient memory */
                exit(1);
            }
            dead_array_size = QTY_BLOCKS;
        }
        ptr = &dead_array[--dead_array_size];
    }
    ptr->bits = bits;
    ptr->index = index;
    ptr->offset = offset;
    if (chain)
        chain->references++;
    ptr->chain = chain;
    ptr->references = 0;
    return ptr;
}

__attribute__((always_inline)) inline void zx0_assign(BLOCK **ptr, BLOCK *chain) {
    chain->references++;
    if (*ptr && !--(*ptr)->references) {
        (*ptr)->ghost_chain = ghost_root;
        ghost_root = *ptr;
    }
    *ptr = chain;
}
