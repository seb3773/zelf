#include "lzfse_internal.h"
#include "lzfse_stub.h"
/*
 * Nostdlib modification:
 * - Removed malloc/free
 * - scratch_buffer is mandatory
 */

// We need memset. If we are truly nostdlib we might need to provide it or rely
// on compiler builtin / external definition. For now, let's assume the user
// links against a mini-stdlib or provides it. To be safe in a stub, we can
// implement a tiny one if needed, but often stubs have utils. The user said
// "use syscalls... or do everything in place". lzfse_decode_base uses memset to
// zero the state. We'll leave memset usage but include string.h. If strict
// nostdlib is needed, one should provide -fno-builtin and implements memset.
// For this step, we assume standard headers for types/mem functions are
// available OR provided by the context. However, the prompt said "Avoid
// malloc/free, stdio...". It didn't strictly forbid string.h for memcpy/memset,
// which are essential. We will include string.h but NOT stdlib.h.
#include <string.h>

size_t lzfse_decode_scratch_size() { return sizeof(lzfse_decoder_state); }

size_t lzfse_decode_buffer(uint8_t *__restrict dst_buffer, size_t dst_size,
                           const uint8_t *__restrict src_buffer,
                           size_t src_size, void *__restrict scratch_buffer) {

  // STRICT REQUIREMENT: scratch_buffer must be provided.
  if (scratch_buffer == NULL) {
    return 0; // Failure
  }

  lzfse_decoder_state *s = (lzfse_decoder_state *)scratch_buffer;
  memset(s, 0x00, sizeof(*s));

  // Initialize state
  s->src = src_buffer;
  s->src_begin = src_buffer;
  s->src_end = s->src + src_size;
  s->dst = dst_buffer;
  s->dst_begin = dst_buffer;
  s->dst_end = dst_buffer + dst_size;

  // Decode
  int status = lzfse_decode(s);
  if (status == LZFSE_STATUS_DST_FULL)
    return dst_size;
  if (status != LZFSE_STATUS_OK)
    return 0;                           // failed
  return (size_t)(s->dst - dst_buffer); // bytes written
}
