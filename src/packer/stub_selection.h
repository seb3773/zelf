#ifndef STUB_SELECTION_H
#define STUB_SELECTION_H
#include "packer_defs.h"

void select_embedded_stub(const char *codec, int mode, int password,
                          const unsigned char **start,
                          const unsigned char **end);

void select_embedded_sfx_stub(const char *codec, const unsigned char **start,
                              const unsigned char **end);

#endif
