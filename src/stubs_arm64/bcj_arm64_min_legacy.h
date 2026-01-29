#ifndef BCJ_ARM64_MIN_LEGACY_H
#define BCJ_ARM64_MIN_LEGACY_H

#include <stddef.h>
#include <stdint.h>

size_t bcj_arm64_decode(uint8_t *buf, size_t size, uint64_t start_ip);

#endif
