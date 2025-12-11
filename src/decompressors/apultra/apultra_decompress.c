/*
 * Apultra Decompressor - Standalone version
 * Extracted decompression-only code from expand.c
 * Copyright (C) 2019 Emmanuel Marty
 */

#include "apultra_decompress.h"
#include <stdlib.h>
#include <string.h>

#define MIN_OFFSET 1
#define MAX_OFFSET 0x1fffff
#define MINMATCH3_OFFSET 1280
#define MINMATCH4_OFFSET 32000

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE __attribute__((always_inline))
#endif

static inline FORCE_INLINE int apultra_read_bit(const unsigned char **ppInBlock, const unsigned char *pDataEnd, int *nCurBitMask, unsigned char *bits) {
   const unsigned char *pInBlock = *ppInBlock;
   int nBit;

   if ((*nCurBitMask) == 0) {
      if (pInBlock >= pDataEnd) return -1;
      (*bits) = *pInBlock++;
      (*nCurBitMask) = 128;
   }

   nBit = ((*bits) & 128) ? 1 : 0;

   (*bits) <<= 1;
   (*nCurBitMask) >>= 1;

   *ppInBlock = pInBlock;
   return nBit;
}

static inline FORCE_INLINE int apultra_read_gamma2(const unsigned char **ppInBlock, const unsigned char *pDataEnd, int *nCurBitMask, unsigned char *bits) {
   int bit;
   unsigned int v = 1;

   do {
      v = (v << 1) + apultra_read_bit(ppInBlock, pDataEnd, nCurBitMask, bits);
      bit = apultra_read_bit(ppInBlock, pDataEnd, nCurBitMask, bits);
      if (bit < 0) return bit;
   } while (bit);

   return v;
}

size_t apultra_decompress(const unsigned char *pInputData, unsigned char *pOutData, size_t nInputSize, size_t nMaxOutBufferSize, size_t nDictionarySize, unsigned int nFlags) {
   const unsigned char *pInputDataEnd = pInputData + nInputSize;
   unsigned char *pCurOutData = pOutData + nDictionarySize;
   const unsigned char *pOutDataEnd = pCurOutData + nMaxOutBufferSize;
   const unsigned char *pOutDataFastEnd = pOutDataEnd - 20;
   int nCurBitMask = 0;
   unsigned char bits = 0;
   int nMatchOffset = -1;
   int nFollowsLiteral = 3;

   if (pInputData >= pInputDataEnd && pCurOutData < pOutDataEnd)
      return -1;
   *pCurOutData++ = *pInputData++;

   while (1) {
      int nResult;

      nResult = apultra_read_bit(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
      if (nResult < 0) return -1;

      if (!nResult) {
         /* '0': literal */
         if (pInputData < pInputDataEnd && pCurOutData < pOutDataEnd) {
            *pCurOutData++ = *pInputData++;
            nFollowsLiteral = 3;
         }
         else {
            return -1;
         }
      }
      else {
         nResult = apultra_read_bit(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
         if (nResult < 0) return -1;

         if (nResult == 0) {
            unsigned int nMatchLen;

            /* '10': 8+n bits offset */
            int nMatchOffsetHi = apultra_read_gamma2(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
            nMatchOffsetHi -= nFollowsLiteral;
            if (nMatchOffsetHi >= 0) {
               nMatchOffset = ((unsigned int) nMatchOffsetHi) << 8;
               nMatchOffset |= (unsigned int)(*pInputData++);

               nMatchLen = apultra_read_gamma2(&pInputData, pInputDataEnd, &nCurBitMask, &bits);

               if (nMatchOffset < 128 || nMatchOffset >= MINMATCH4_OFFSET)
                  nMatchLen += 2;
               else if (nMatchOffset >= MINMATCH3_OFFSET)
                  nMatchLen++;
            }
            else {
               /* else rep-match */
               nMatchLen = apultra_read_gamma2(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
            }

            nFollowsLiteral = 2;
            const unsigned char *pSrc = pCurOutData - nMatchOffset;
            if (pSrc >= pOutData && (pSrc + nMatchLen) <= pOutDataEnd) {
               if (nMatchLen < 11 && nMatchOffset >= 8 && pCurOutData < pOutDataFastEnd) {
                  memcpy(pCurOutData, pSrc, 8);
                  memcpy(pCurOutData + 8, pSrc + 8, 2);
                  pCurOutData += nMatchLen;
               }
               else {
                  if ((pCurOutData + nMatchLen) <= pOutDataEnd) {
                     /* Do a deterministic, left to right byte copy instead of memcpy() so as to handle overlaps */

                     if (nMatchOffset >= 16 && (pCurOutData + nMatchLen) < (pOutDataFastEnd - 15)) {
                        const unsigned char *pCopySrc = pSrc;
                        unsigned char *pCopyDst = pCurOutData;
                        const unsigned char *pCopyEndDst = pCurOutData + nMatchLen;

                        do {
                           memcpy(pCopyDst, pCopySrc, 16);
                           pCopySrc += 16;
                           pCopyDst += 16;
                        } while (pCopyDst < pCopyEndDst);

                        pCurOutData += nMatchLen;
                     }
                     else {
                        while (nMatchLen) {
                           *pCurOutData++ = *pSrc++;
                           nMatchLen--;
                        }
                     }
                  }
                  else {
                     return -1;
                  }
               }
            }
            else {
               return -1;
            }
         }
         else {
            nResult = apultra_read_bit(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
            if (nResult < 0) return -1;

            if (nResult == 0) {
               unsigned int nCommand;
               unsigned int nMatchLen;

               /* '110': 7 bits offset + 1 bit length */
               nCommand = (unsigned int)(*pInputData++);
               if (nCommand == 0x00) {
                  /* EOD. No match len follows. */
                  break;
               }

               /* Bits 7-1: offset; bit 0: length */
               nMatchOffset = (nCommand >> 1);
               nMatchLen = (nCommand & 1) + 2;

               nFollowsLiteral = 2;
               const unsigned char *pSrc = pCurOutData - nMatchOffset;
               if (pSrc >= pOutData && (pSrc + nMatchLen) <= pOutDataEnd) {
                  if (nMatchOffset >= 8 && pCurOutData < pOutDataFastEnd) {
                     memcpy(pCurOutData, pSrc, 8);
                     memcpy(pCurOutData + 8, pSrc + 8, 2);
                     pCurOutData += nMatchLen;
                  }
                  else {
                     if ((pCurOutData + nMatchLen) <= pOutDataEnd) {
                        while (nMatchLen) {
                           *pCurOutData++ = *pSrc++;
                           nMatchLen--;
                        }
                     }
                     else {
                        return -1;
                     }
                  }
               }
               else {
                  return -1;
               }
            }
            else {
               unsigned int nShortMatchOffset;

               /* '111': 4 bit offset */
               nResult = apultra_read_bit(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
               if (nResult < 0) return -1;
               nShortMatchOffset = nResult << 3;

               nResult = apultra_read_bit(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
               if (nResult < 0) return -1;
               nShortMatchOffset |= nResult << 2;

               nResult = apultra_read_bit(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
               if (nResult < 0) return -1;
               nShortMatchOffset |= nResult << 1;

               nResult = apultra_read_bit(&pInputData, pInputDataEnd, &nCurBitMask, &bits);
               if (nResult < 0) return -1;
               nShortMatchOffset |= nResult << 0;

               nFollowsLiteral = 3;
               if (nShortMatchOffset) {
                  /* Short offset, 1-15 */
                  const unsigned char *pSrc = pCurOutData - nShortMatchOffset;
                  if (pSrc >= pOutData && (pCurOutData + 1) <= pOutDataEnd && (pSrc + 1) <= pOutDataEnd) {
                     *pCurOutData++ = *pSrc++;
                  }
                  else {
                     return -1;
                  }
               }
               else {
                  /* Write zero */
                  if ((pCurOutData + 1) <= pOutDataEnd) {
                     *pCurOutData++ = 0;
                  }
                  else {
                     return -1;
                  }
               }
            }
         }
      }
   }

   return (size_t)(pCurOutData - pOutData) - nDictionarySize;
}
