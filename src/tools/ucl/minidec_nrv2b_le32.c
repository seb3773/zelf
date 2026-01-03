// Minimal NRV2B LE32 decompressor (trimmed from UCL n2b_d.c + getbit.h).
// Standalone, no libc dependencies beyond basic integer types.

#include "minidec_nrv2b_le32.h"

#define getbit_le32(bb, bc, src, ilen, src_len, input_err)                \
    ((bc) > 0 ? (((bb) >> --(bc)) & 1u) :                                 \
     (((ilen) + 4u > (src_len)) ? ((input_err) = UCL_E_INPUT_OVERRUN, 0u) : \
      ((bc) = 31u,                                                        \
       (bb) = (uint32_t)(src)[(ilen)] + ((uint32_t)(src)[(ilen) + 1] << 8) + \
              ((uint32_t)(src)[(ilen) + 2] << 16) + ((uint32_t)(src)[(ilen) + 3] << 24), \
       (ilen) += 4u, (((bb) >> 31) & 1u))))

int ucl_nrv2b_decompress_le32(const ucl_byte *src, ucl_uint src_len,
                              ucl_byte *dst, ucl_uintp dst_len) {
    uint32_t bb = 0;
    uint32_t bc = 0;
    ucl_uint ilen = 0, olen = 0, last_m_off = 1;
    const ucl_uint oend = *dst_len;
    int input_err = UCL_E_OK;

    ucl_unused_u(src);

    for (;;) {
        ucl_uint m_off, m_len;

        while (getbit_le32(bb, bc, src, ilen, src_len, input_err)) {
            if (input_err != UCL_E_OK) return input_err;
            if (olen >= oend) return UCL_E_OUTPUT_OVERRUN;
            dst[olen++] = src[ilen++];
        }
        m_off = 1;
        do {
            m_off = (m_off * 2u) + getbit_le32(bb, bc, src, ilen, src_len, input_err);
            if (input_err != UCL_E_OK) return input_err;
            if (m_off > UCL_UINT32_C(0xffffff) + 3) return UCL_E_LOOKBEHIND_OVERRUN;
        } while (!getbit_le32(bb, bc, src, ilen, src_len, input_err));
        if (input_err != UCL_E_OK) return input_err;
        if (m_off == 2) {
            m_off = last_m_off;
        } else {
            if (ilen >= src_len) return UCL_E_INPUT_OVERRUN;
            m_off = (m_off - 3u) * 256u + src[ilen++];
            if (m_off == UCL_UINT32_C(0xffffffff)) break;
            last_m_off = ++m_off;
        }
        m_len = getbit_le32(bb, bc, src, ilen, src_len, input_err);
        if (input_err != UCL_E_OK) return input_err;
        m_len = (m_len * 2u) + getbit_le32(bb, bc, src, ilen, src_len, input_err);
        if (input_err != UCL_E_OK) return input_err;
        if (m_len == 0) {
            m_len++;
            do {
                m_len = (m_len * 2u) + getbit_le32(bb, bc, src, ilen, src_len, input_err);
                if (input_err != UCL_E_OK) return input_err;
                if (m_len >= oend) return UCL_E_OUTPUT_OVERRUN;
            } while (!getbit_le32(bb, bc, src, ilen, src_len, input_err));
            if (input_err != UCL_E_OK) return input_err;
            m_len += 2;
        }
        m_len += (m_off > 0xd00u);
        if (olen + m_len > oend) return UCL_E_OUTPUT_OVERRUN;
        if (m_off > olen) return UCL_E_LOOKBEHIND_OVERRUN;
        {
            const ucl_byte *m_pos = dst + olen - m_off;
            dst[olen++] = *m_pos++;
            do dst[olen++] = *m_pos++; while (--m_len > 0);
        }
    }
    *dst_len = olen;
    return (ilen == src_len) ? UCL_E_OK : (ilen < src_len ? UCL_E_INPUT_NOT_CONSUMED : UCL_E_INPUT_OVERRUN);
}

// Non-inline wrapper used by the stage-0 asm
int stage0_nrv2b_le32(const uint8_t *src, uint32_t src_len,
                      uint8_t *dst, uint32_t *dst_len) {
    return ucl_nrv2b_decompress_le32(src, src_len, dst, (ucl_uintp)dst_len);
}


