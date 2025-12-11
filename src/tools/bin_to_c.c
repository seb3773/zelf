#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("static const unsigned char compressed_data[] = {\n");

    size_t bytes_written = 0;
    int byte;
    while ((byte = fgetc(file)) != EOF) {
        if (bytes_written % 12 == 0) {
            printf("    ");
        }
        printf("0x%02x", byte);
        bytes_written++;

        if (bytes_written < size) {
            printf(",");
        }

        if (bytes_written % 12 == 0 || bytes_written == size) {
            printf("\n");
        } else {
            printf(" ");
        }
    }

    printf("};\n");
    printf("static const size_t compressed_size = %zu;\n", size);

    fclose(file);
    return 0;
}