#include "kanzi_exe_encode_c.h"
#include "kanzi_exe_encode.hpp"

extern "C" {

int kanzi_exe_get_max_encoded_length(int src_len) {
    return kanzi::KanziEXEFilterEncode::getMaxEncodedLength(src_len);
}

int kanzi_exe_filter_encode_force_x86_range(const unsigned char* in, int in_size,
                                            unsigned char* out, int out_capacity,
                                            int* processed_size,
                                            int code_start, int code_end) {
    kanzi::KanziEXEFilterEncode enc;
    int processed = 0;
    int out_size = enc.forwardForceX86Range((const kanzi::byte*)in, in_size,
                                            (kanzi::byte*)out, out_capacity,
                                            processed,
                                            code_start, code_end);
    if (processed_size)
        *processed_size = processed;
    return out_size;
}

int kanzi_exe_filter_encode(const unsigned char* in, int in_size,
                            unsigned char* out, int out_capacity,
                            int* processed_size) {
    kanzi::KanziEXEFilterEncode enc;
    int processed = 0;
    int out_size = enc.forward((const kanzi::byte*)in, in_size,
                               (kanzi::byte*)out, out_capacity,
                               processed);
    if (processed_size)
        *processed_size = processed;
    return out_size; // 0 on failure per API
}

} // extern "C"
