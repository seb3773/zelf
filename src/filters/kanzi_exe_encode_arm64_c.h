#ifndef KANZI_EXE_ENCODE_ARM64_C_H
#define KANZI_EXE_ENCODE_ARM64_C_H

#ifdef __cplusplus
extern "C" {
#endif

// C wrapper around Kanzi EXE filter encoder (encode-only) - ARM64 version
// Returns size of encoded output (>0) on success, 0 on failure
int kanzi_exe_filter_encode_arm64(const unsigned char* in, int in_size,
                            unsigned char* out, int out_capacity,
                            int* processed_size);

int kanzi_exe_filter_encode_force_arm64_range(const unsigned char* in, int in_size,
                                              unsigned char* out, int out_capacity,
                                              int* processed_size,
                                              int code_start, int code_end);

// Helper: maximum encoded length for pre-allocation
int kanzi_exe_get_max_encoded_length(int src_len);

#ifdef __cplusplus
}
#endif

#endif /* KANZI_EXE_ENCODE_ARM64_C_H */
