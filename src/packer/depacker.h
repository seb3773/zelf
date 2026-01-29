#ifndef DEPACKER_H
#define DEPACKER_H

#include <stddef.h>

/**
 * Unpacks an ELFZ-packed ELF binary.
 *
 * @param input_path Path to the packed ELF file.
 * @param output_path Path to write the reconstructed ELF file.
 * @return 0 on success, non-zero on failure.
 */
int elfz_depack(const char *input_path, const char *output_path);

#endif // DEPACKER_H
