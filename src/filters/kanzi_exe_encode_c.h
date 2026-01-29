#ifndef KANZI_EXE_ENCODE_C_H
#define KANZI_EXE_ENCODE_C_H

#ifdef __cplusplus
extern "C" {
#endif

// C wrapper around Kanzi EXE filter encoder (encode-only)
// Returns size of encoded output (>0) on success, 0 on failure
int kanzi_exe_filter_encode(const unsigned char* in, int in_size,
                            unsigned char* out, int out_capacity,
                            int* processed_size);

// Helper: maximum encoded length for pre-allocation
int kanzi_exe_get_max_encoded_length(int src_len);

// Force x86 filtering on a specified [code_start, code_end) range (file offsets)
// Returns size of encoded output (>0) on success, 0 on failure
int kanzi_exe_filter_encode_force_x86_range(const unsigned char* in, int in_size,
                                            unsigned char* out, int out_capacity,
                                            int* processed_size,
                                            int code_start, int code_end);

#ifdef __cplusplus
}
#endif

#endif /* KANZI_EXE_ENCODE_C_H */
