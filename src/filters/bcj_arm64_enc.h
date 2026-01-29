#ifndef BCJ_ARM64_ENC_H
#define BCJ_ARM64_ENC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t bcj_arm64_encode(uint8_t *buffer, size_t size, uint64_t start_ip);

#ifdef __cplusplus
}
#endif

#endif /* BCJ_ARM64_ENC_H */
