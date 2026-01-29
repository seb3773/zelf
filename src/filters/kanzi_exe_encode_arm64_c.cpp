#include "kanzi_exe_encode_arm64_c.h"
#include "kanzi_exe_encode.hpp"

extern "C" {

int kanzi_exe_filter_encode_arm64(const unsigned char* in, int in_size,
                            unsigned char* out, int out_capacity,
                            int* processed_size) {
    if (!processed_size) return 0;
    
    kanzi::KanziEXEFilterEncode encoder;
    int procSize = 0;
    int res = 0;
    
    // Use standard forward() method as requested.
    // Ideally this will detect ARM64 on an ARM64 system.
    res = encoder.forward(in, in_size, out, out_capacity, procSize);
    
    *processed_size = procSize;
    return res;
}

int kanzi_exe_filter_encode_force_arm64_range(const unsigned char* in, int in_size,
                                              unsigned char* out, int out_capacity,
                                              int* processed_size,
                                              int code_start, int code_end) {
    if (!processed_size) return 0;

    kanzi::KanziEXEFilterEncode encoder;
    int procSize = 0;
    int res = encoder.forwardForceARMRange((const kanzi::byte*)in, in_size,
                                           (kanzi::byte*)out, out_capacity,
                                           procSize,
                                           code_start, code_end);

    *processed_size = procSize;
    return res;
}

}
