#ifndef ARCHIVE_MODE_H
#define ARCHIVE_MODE_H

#include <stddef.h>
#include <stdint.h>

// Archive/Unpack functions
int archive_file(const char *input_path, const char *output_path,
                 const char *codec);
int archive_tar_dir(const char *dir_path, const char *output_path,
                    const char *codec);
int archive_sfx_file(const char *input_path, const char *output_path,
                     const char *codec);
int archive_sfx_tar_dir(const char *dir_path, const char *output_path,
                        const char *codec);
int archive_list(const char *input_path);
int unpack_file(const char *input_path, const char *output_path);

#endif // ARCHIVE_MODE_H
