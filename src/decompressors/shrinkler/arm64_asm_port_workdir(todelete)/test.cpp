#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "comp/shrinkler_comp.h"
#include "decomp/codec_shrinkler.h"

int main() {
    const char* input = "Hello world! This is a test string to verify Shrinkler compression and decompression. It needs to be long enough to actually compress something useful. Adding more text to make it longer and have some repetitions. Hello world! This is a test string.";
    size_t input_len = strlen(input) + 1; // Include null terminator
    
    printf("Input size: %zu\n", input_len);
    
    uint8_t* compressed_data = NULL;
    size_t compressed_size = 0;
    
    shrinkler_comp_params params;
    memset(&params, 0, sizeof(params));
    params.iterations = 1; // Fast
    
    // Compress
    if (!shrinkler_compress_memory((const uint8_t*)input, input_len, &compressed_data, &compressed_size, &params)) {
        printf("Compression failed!\n");
        return 1;
    }
    printf("Compressed size: %zu\n", compressed_size);
    
    // Check header
    if (compressed_size < 24) {
         printf("Compressed data too small for header\n");
         return 1;
    }
    
    const uint8_t* p = compressed_data;
    uint32_t uncomp_len = (p[12]<<24) | (p[13]<<16) | (p[14]<<8) | p[15];
    uint32_t margin = (p[16]<<24) | (p[17]<<16) | (p[18]<<8) | p[19];
    
    printf("Header uncompressed size: %u\n", uncomp_len);
    printf("Header margin: %u\n", margin);
    
    if (uncomp_len != input_len) {
        printf("Size mismatch in header!\n");
        return 1;
    }
    
    size_t dst_cap = uncomp_len + margin + 4096; 
    char* decompressed_data = (char*)malloc(dst_cap);
    if (!decompressed_data) {
        printf("Malloc failed\n");
        return 1;
    }
    
    int res = shrinkler_decompress_c((const char*)compressed_data, decompressed_data, (int)compressed_size, (int)dst_cap);
    printf("Decompression result: %d\n", res);
    
    if (res != (int)uncomp_len) {
        printf("Decompression failed or size mismatch! res=%d expected=%u\n", res, uncomp_len);
        return 1;
    }
    
    if (memcmp(input, decompressed_data, input_len) == 0) {
        printf("Verification SUCCESS!\n");
        return 0;
    } else {
        printf("Verification FAILED! Content mismatch.\n");
        return 1;
    }
}
