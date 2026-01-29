#ifndef BCJ_ARM64_MIN_H
#define BCJ_ARM64_MIN_H

#include <stddef.h>
#include <stdint.h>

static inline uint32_t bcj_arm64_read32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static inline void bcj_arm64_write32le(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static inline size_t bcj_arm64_decode(uint8_t *buf, size_t size,
                                     uint64_t start_ip) {
  if (!buf)
    return 0;

  size &= ~(size_t)3;

  for (size_t i = 0; i < size; i += 4) {
    uint32_t instr = bcj_arm64_read32le(buf + i);

    const uint32_t op6 = instr >> 26;
    if (op6 == 0x05 || op6 == 0x25) {
      uint32_t imm26 = instr & 0x03FFFFFFu;
      uint32_t adj = (uint32_t)((start_ip + (uint64_t)i) >> 2);
      imm26 = (imm26 - adj) & 0x03FFFFFFu;
      instr = (instr & 0xFC000000u) | imm26;
      bcj_arm64_write32le(buf + i, instr);
      continue;
    }

    if ((instr & 0x9F000000u) == 0x90000000u) {
      uint32_t addr = ((instr >> 29) & 3u) | ((instr >> 3) & 0x1FFFFCu);

      if ((addr + 0x020000u) & 0x1C0000u)
        continue;

      addr -= (uint32_t)((start_ip + (uint64_t)i) >> 12);

      instr &= 0x9000001Fu;
      instr |= (addr & 3u) << 29;
      instr |= (addr & 0x03FFFCu) << 3;
      instr |= (0u - (addr & 0x020000u)) & 0xE00000u;

      bcj_arm64_write32le(buf + i, instr);
      continue;
    }
  }

  return size;
}

#endif /* BCJ_ARM64_MIN_H */
