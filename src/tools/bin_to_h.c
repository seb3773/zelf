/*
 * Binary-to-C-header converter
 * Usage: ./bin_to_h file.lz4 > compressed_data.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.lz4>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("Error opening file");
        return 1;
    }

    // Get compressed file size
    struct stat st;
    stat(argv[1], &st);
    size_t compressed_size = st.st_size;

    // Try to deduce original size from source file name
    char original_name[256];
    strncpy(original_name, argv[1], sizeof(original_name)-1);
    original_name[sizeof(original_name)-1] = '\0';

    char *dot = strrchr(original_name, '.');
    if (dot) *dot = '\0'; // remove .lz4

    struct stat orig_st;
    size_t original_size = compressed_size * 3; // fallback if not found
    if (stat(original_name, &orig_st) == 0) {
        original_size = orig_st.st_size;
    }

    // Emit header
    printf("/*\n");
    printf(" * LZ4 compressed binary data - Auto-generated\n");
    printf(" * Source file: %s\n", argv[1]);
    printf(" * Compressed size: %zu bytes\n", compressed_size);
    printf(" * Original size: %zu bytes\n", original_size);
    printf(" */\n\n");

    printf("#ifndef COMPRESSED_QXTASK_H\n");
    printf("#define COMPRESSED_QXTASK_H\n\n");

    printf("static const size_t compressed_qxtask_size = %zu;\n", compressed_size);
    printf("static const size_t original_qxtask_size = %zu;\n\n", original_size);

    printf("static const unsigned char compressed_qxtask_data[] = {\n");

    // Read and convert to a C array (16 bytes per line)
    unsigned char buffer[16];
    size_t total = 0;

    while (total < compressed_size) {
        size_t to_read = (compressed_size - total > 16) ? 16 : (compressed_size - total);
        size_t read_count = fread(buffer, 1, to_read, f);
        if (read_count == 0) break;

        printf("    ");
        for (size_t i = 0; i < read_count; i++) {
            printf("0x%02x", buffer[i]);
            if (total + i + 1 < compressed_size) printf(",");
            if (i < read_count - 1) printf(" ");
        }
        printf("\n");

        total += read_count;
    }

    printf("};\n\n");
    printf("#endif /* COMPRESSED_QXTASK_H */\n");

    fclose(f);
    return 0;
}