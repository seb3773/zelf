// lzham_comp_shim.cpp - C++ shim exposing only compression C API, to avoid
// linking lzham_lib.cpp's decompressor Linux x86_64. Build as part of
// liblzhamc_wrap.a

#include "lzham.h"
#include "lzham_comp.h"

extern "C" {

lzham_compress_state_ptr LZHAM_CDECL
lzham_compress_init(const lzham_compress_params *pParams) {
  return lzham::lzham_lib_compress_init(pParams);
}

lzham_compress_state_ptr LZHAM_CDECL
lzham_compress_reinit(lzham_compress_state_ptr pState) {
  return lzham::lzham_lib_compress_reinit(pState);
}

lzham_uint32 LZHAM_CDECL
lzham_compress_deinit(lzham_compress_state_ptr pState) {
  return lzham::lzham_lib_compress_deinit(pState);
}

lzham_compress_status_t LZHAM_CDECL
lzham_compress(lzham_compress_state_ptr pState, const lzham_uint8 *pIn_buf,
               size_t *pIn_buf_size, lzham_uint8 *pOut_buf,
               size_t *pOut_buf_size, lzham_bool no_more_input_bytes_flag) {
  return lzham::lzham_lib_compress(pState, pIn_buf, pIn_buf_size, pOut_buf,
                                   pOut_buf_size, no_more_input_bytes_flag);
}

lzham_compress_status_t LZHAM_CDECL
lzham_compress2(lzham_compress_state_ptr pState, const lzham_uint8 *pIn_buf,
                size_t *pIn_buf_size, lzham_uint8 *pOut_buf,
                size_t *pOut_buf_size, lzham_flush_t flush_type) {
  return lzham::lzham_lib_compress2(pState, pIn_buf, pIn_buf_size, pOut_buf,
                                    pOut_buf_size, flush_type);
}

lzham_compress_status_t LZHAM_CDECL lzham_compress_memory(
    const lzham_compress_params *pParams, lzham_uint8 *pDst_buf,
    size_t *pDst_len, const lzham_uint8 *pSrc_buf, size_t src_len,
    lzham_uint32 *pAdler32) {
  return lzham::lzham_lib_compress_memory(pParams, pDst_buf, pDst_len, pSrc_buf,
                                          src_len, pAdler32);
}

} // extern "C"
