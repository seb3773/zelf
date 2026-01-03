/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#ifndef ZSTD_DDICT_NOOP_H
#define ZSTD_DDICT_NOOP_H

#if ZSTD_DECOMPRESS_DICTIONARY == 0
#if ZSTD_ADD_UNUSED
MEM_STATIC FORCE_INLINE_ATTR void ZSTD_clearDict(ZSTD_DCtx* dctx) { (void)dctx; }
#endif
#if ZSTD_ADD_UNUSED
MEM_STATIC FORCE_INLINE_ATTR size_t ZSTD_DCtx_refDDict(ZSTD_DCtx* dctx, const ZSTD_DDict* ddict) { (void)dctx; (void)ddict; return 0; }
#endif
MEM_STATIC FORCE_INLINE_ATTR size_t ZSTD_checkOutBuffer(ZSTD_DStream const* zds, ZSTD_outBuffer const* output) { (void)zds;  (void)output; return 0; }
MEM_STATIC FORCE_INLINE_ATTR ZSTD_DDict const* ZSTD_getDDict(ZSTD_DCtx* dctx) { (void)dctx; return NULL; }
#endif

#endif /* ZSTD_DDICT_NOOP_H */
