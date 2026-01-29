#include "minlzma_core.h"

/* Tiny LZMA decoder core for Linux x86_64 stub use.
 * Constraints / assumptions:
 * - Supports LZMA raw stream prefixed by 5-byte props (lc,lp,pb,dict).
 * - We target fixed props from our packer: lc=3, lp=0, pb=2. Caller must enforce.
 * - External dictionary: dst buffer acts as dictionary window; no extra window.
 * - No dynamic memory except small probability arrays allocated on stack.
 * - Defensive bounds checks to avoid buffer overruns.
 * - Returns number of bytes written to dst, or -1 on error.
 *
 * This is a trimmed implementation based on the standard LZMA range decoder
 * and state machine, reduced to the features we use. Probabilities are kept
 * as 11-bit values (0..2047).
 */

#include <stdint.h>

typedef uint16_t CLzmaProb;

/* Constants taken from LZMA spec */
#define kNumTopBits 24
#define kTopValue ((uint32_t)1 << kNumTopBits)

#define kNumBitModelTotalBits 11
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5

#define kNumPosBitsMax 4
#define kNumPosStatesMax (1 << kNumPosBitsMax)

#define kLenNumLowBits 3
#define kLenNumLowSymbols (1 << kLenNumLowBits)
#define kLenNumHighBits 8
#define kLenNumHighSymbols (1 << kLenNumHighBits)

#define kNumStates 12
#define kNumLitStates 7

#define kStartPosModelIndex 4
#define kEndPosModelIndex 14
#define kNumFullDistances (1 << (kEndPosModelIndex >> 1)) /* 128 */

#define kNumPosSlotBits 6
#define kNumLenToPosStates 4

#define kNumAlignBits 4
#define kAlignTableSize (1 << kNumAlignBits)

#define kMatchMinLen 2

/* State update helpers (from 7-Zip) */
static inline unsigned StateInit(void) { return 0; }
static inline void StateUpdateChar(unsigned *state) {
    if (*state < 4) *state = 0; else if (*state < 10) *state -= 3; else *state -= 6;
}
static inline void StateUpdateMatch(unsigned *state) { *state = (*state < 7) ? 7 : 10; }
static inline void StateUpdateRep(unsigned *state) { *state = (*state < 7) ? 8 : 11; }
static inline void StateUpdateShortRep(unsigned *state) { *state = (*state < 7) ? 9 : 11; }
static inline int StateIsCharState(unsigned state) { return state < 7; }

static inline unsigned GetLenToPosState(unsigned len) {
    len -= kMatchMinLen;
    if (len < kNumLenToPosStates) return len;
    return kNumLenToPosStates - 1;
}

/* Range decoder */
typedef struct {
    const uint8_t *buf;
    const uint8_t *bufLimit;
    uint32_t range;
    uint32_t code;
} RangeDec;

static inline int RC_Init(RangeDec *rd, const uint8_t *src, int srcLen) {
    if (srcLen < 5) return -1;
    rd->buf = src;
    rd->bufLimit = src + srcLen;
    rd->range = 0xFFFFFFFFu;
    rd->code = 0;
    for (int i = 0; i < 5; i++) {
        rd->code = (rd->code << 8) | *rd->buf++;
    }
    return 0;
}

#define RC_NORMALIZE(rd) do { \
    if ((rd)->range < kTopValue) { \
        if ((rd)->buf >= (rd)->bufLimit) return -1; \
        (rd)->range <<= 8; \
        (rd)->code = ((rd)->code << 8) | *((rd)->buf++); \
    } \
} while (0)

static inline unsigned RC_Bit(RangeDec *rd, CLzmaProb *prob) {
    RC_NORMALIZE(rd);
    uint32_t bound = (rd->range >> kNumBitModelTotalBits) * (uint32_t)(*prob);
    unsigned t;
    if (rd->code < bound) {
        rd->range = bound;
        *prob = (CLzmaProb)(*prob + ((kBitModelTotal - *prob) >> kNumMoveBits));
        t = 0;
    } else {
        rd->range -= bound;
        rd->code -= bound;
        *prob = (CLzmaProb)(*prob - (*prob >> kNumMoveBits));
        t = 1;
    }
    return t;
}

static inline uint32_t RC_DirectBits(RangeDec *rd, unsigned numBits) {
    uint32_t res = 0;
    for (unsigned i = 0; i < numBits; i++) {
        RC_NORMALIZE(rd);
        rd->range >>= 1;
        uint32_t t = (rd->code - rd->range) >> 31; /* 0 if code < range */
        rd->code -= rd->range & (t - 1);
        res = (res << 1) | (1 - t);
    }
    return res;
}

static inline unsigned Tree_Decode(RangeDec *rd, CLzmaProb *probs, unsigned numBits) {
    unsigned symbol = 1;
    for (unsigned i = 0; i < numBits; i++) {
        symbol = (symbol << 1) | RC_Bit(rd, &probs[symbol]);
    }
    return symbol - (1u << numBits);
}

static inline unsigned Tree_Decode_Rev(RangeDec *rd, CLzmaProb *probs, unsigned numBits) {
    unsigned symbol = 0;
    unsigned m = 1;
    for (unsigned i = 0; i < numBits; i++) {
        unsigned bit = RC_Bit(rd, &probs[m]);
        m <<= 1;
        m += bit;
        symbol |= (bit << i);
    }
    return symbol;
}

/* Length decoder */
typedef struct {
    CLzmaProb Choice;
    CLzmaProb Choice2;
    CLzmaProb Low[kNumPosStatesMax][kLenNumLowSymbols];
    CLzmaProb Mid[kNumPosStatesMax][kLenNumLowSymbols];
    CLzmaProb High[kLenNumHighSymbols];
} LenDecoder;

static inline void Len_Init(LenDecoder *ld) {
    ld->Choice = ld->Choice2 = 1024;
    for (unsigned ps = 0; ps < kNumPosStatesMax; ps++) {
        for (unsigned i = 0; i < kLenNumLowSymbols; i++) {
            ld->Low[ps][i] = 1024;
            ld->Mid[ps][i] = 1024;
        }
    }
    for (unsigned i = 0; i < kLenNumHighSymbols; i++) ld->High[i] = 1024;
}

static inline unsigned Len_Decode(LenDecoder *ld, RangeDec *rd, unsigned posState) {
    if (RC_Bit(rd, &ld->Choice) == 0) {
        return Tree_Decode(rd, ld->Low[posState], kLenNumLowBits) + kMatchMinLen;
    }
    if (RC_Bit(rd, &ld->Choice2) == 0) {
        return Tree_Decode(rd, ld->Mid[posState], kLenNumLowBits) + kMatchMinLen + kLenNumLowSymbols;
    }
    return Tree_Decode(rd, ld->High, kLenNumHighBits) + kMatchMinLen + kLenNumLowSymbols * 2;
}

/* Main tiny decoder */
int minlzma_decode(const unsigned char *src, int srcLen,
                   unsigned char *dst, int dstLen,
                   unsigned lc, unsigned lp, unsigned pb)
{
    if (!src || !dst || srcLen <= 5 || dstLen <= 0) return -1;
    /* Only support our target props for tiny core */
    if (!(lc == 3 && lp == 0 && pb == 2)) return -1;

    /* stream 'src' starts at range-coder bytes; props were parsed by caller */

    RangeDec rd;
    if (RC_Init(&rd, src, srcLen) != 0) return -5;

    /* Probabilities */
    const unsigned posStates = 1u << pb; /* 4 */

    CLzmaProb IsMatch[kNumStates * kNumPosStatesMax];
    CLzmaProb IsRep[kNumStates];
    CLzmaProb IsRepG0[kNumStates];
    CLzmaProb IsRepG1[kNumStates];
    CLzmaProb IsRepG2[kNumStates];
    CLzmaProb IsRep0Long[kNumStates * kNumPosStatesMax];
    CLzmaProb PosSlot[kNumLenToPosStates][1 << kNumPosSlotBits];
    /* Special distances reverse trees table used by REV_BIT_VAR.
     * We reserve 192 entries to safely cover indices up to base+63 when base=96.
     */
    CLzmaProb SpecPos[192];
    CLzmaProb Align[kAlignTableSize];

    /* Literal probs: 0x300 << (lc+lp) with lc=3,lp=0 -> 0x1800 */
    enum { LIT_SIZE = (0x300 << (3 + 0)) };
    CLzmaProb LitProbs[LIT_SIZE];

    for (unsigned i = 0; i < kNumStates * kNumPosStatesMax; i++) {
        IsMatch[i] = 1024;
        IsRep0Long[i] = 1024;
    }
    for (unsigned i = 0; i < kNumStates; i++) {
        IsRep[i] = IsRepG0[i] = IsRepG1[i] = IsRepG2[i] = 1024;
    }
    for (unsigned st = 0; st < kNumLenToPosStates; st++)
        for (unsigned i = 0; i < (1u << kNumPosSlotBits); i++)
            PosSlot[st][i] = 1024;
    for (unsigned i = 0; i < 192; i++) SpecPos[i] = 1024;
    for (unsigned i = 0; i < kAlignTableSize; i++) Align[i] = 1024;
    for (unsigned i = 0; i < LIT_SIZE; i++) LitProbs[i] = 1024;

    LenDecoder LenDec, RepLenDec;
    Len_Init(&LenDec);
    Len_Init(&RepLenDec);

    /* Decoding state */
    unsigned state = StateInit();
    unsigned rep0 = 1, rep1 = 1, rep2 = 1, rep3 = 1;
    int dicPos = 0;

    while (dicPos < dstLen) {
        unsigned posClass = (unsigned)dicPos & (posStates - 1);
        unsigned posIndex = (posClass << kNumPosBitsMax); /* (processedPos & pbMask) << 4 */
        CLzmaProb *p_IsMatch = &IsMatch[posIndex + state];
        if (RC_Bit(&rd, p_IsMatch) == 0) {
            /* Literal */
            unsigned prevByte = (dicPos > 0) ? dst[dicPos - 1] : 0;
            unsigned litState = (prevByte >> (8 - lc)); /* lp=0 => no pos bits */
            CLzmaProb *prob = &LitProbs[0x300 * litState];
            unsigned symbol = 1;
            if (!StateIsCharState(state)) {
                if ((unsigned)dicPos < rep0) return -2; /* prevent underflow */
                unsigned matchByte = dst[dicPos - rep0];
                unsigned offs = 0x100;
                do {
                    matchByte += matchByte;
                    unsigned bit = offs;
                    offs &= matchByte;
                    CLzmaProb *probLit = &prob[offs + bit + symbol];
                    unsigned b = RC_Bit(&rd, probLit);
                    symbol = (symbol << 1) | b;
                    if (b == 0) offs ^= bit;
                } while (symbol < 0x100);
            } else {
                while (symbol < 0x100) {
                    symbol = (symbol << 1) | RC_Bit(&rd, &prob[symbol]);
                }
            }
            dst[dicPos++] = (unsigned char)symbol;
            StateUpdateChar(&state);
            continue;
        }

        /* Match or Rep */
        CLzmaProb *p_IsRep = &IsRep[state];
        unsigned len;
        if (RC_Bit(&rd, p_IsRep) == 0) {
            /* Match */
            StateUpdateMatch(&state);
            unsigned lenState = Len_Decode(&LenDec, &rd, posClass);
            unsigned posSlot = Tree_Decode(&rd, PosSlot[GetLenToPosState(lenState)], kNumPosSlotBits);
            unsigned dist;
            if (posSlot < kStartPosModelIndex) {
                dist = posSlot;
            } else {
                unsigned numDirectBits = (posSlot >> 1) - 1;
                dist = ((2 | (posSlot & 1)) << numDirectBits);
                if (posSlot < kEndPosModelIndex) {
                    unsigned i = dist + 1;
                    unsigned m = 1;
                    unsigned n = numDirectBits;
                    while (n--) {
                        unsigned b = RC_Bit(&rd, &SpecPos[i]);
                        if (b == 0) { i += m; m += m; }
                        else { m += m; i += m; }
                    }
                    dist = i - m;
                } else {
                    dist += RC_DirectBits(&rd, numDirectBits - kNumAlignBits) << kNumAlignBits;
                    dist += Tree_Decode_Rev(&rd, Align, kNumAlignBits);
                }
            }
            /* Validate distance */
            if ((unsigned)(dist + 1) > (unsigned)dicPos) return -4;
            rep3 = rep2; rep2 = rep1; rep1 = rep0; rep0 = dist + 1; /* store back = distance+1 */
            len = lenState;
        } else {
            /* Rep */
            if (RC_Bit(&rd, &IsRepG0[state]) == 0) {
                if (RC_Bit(&rd, &IsRep0Long[posIndex + state]) == 0) {
                    StateUpdateShortRep(&state);
                    if ((unsigned)dicPos < rep0) return -3;
                    unsigned c = dst[dicPos - rep0];
                    dst[dicPos++] = (unsigned char)c;
                    continue;
                }
            } else {
                unsigned dist;
                if (RC_Bit(&rd, &IsRepG1[state]) == 0) {
                    dist = rep1;
                } else {
                    if (RC_Bit(&rd, &IsRepG2[state]) == 0) {
                        dist = rep2;
                    } else {
                        dist = rep3;
                        rep3 = rep2;
                    }
                    rep2 = rep1;
                }
                rep1 = rep0; rep0 = dist;
            }
            StateUpdateRep(&state);
            len = Len_Decode(&RepLenDec, &rd, posClass);
        }

        /* Copy match */
        if (rep0 > (unsigned)dicPos) return -4; /* invalid distance */
        unsigned back = rep0;
        unsigned rem = dstLen - dicPos;
        if (len > rem) len = rem;
        /* Efficient small copy */
        while (len--) {
            dst[dicPos] = dst[dicPos - back];
            dicPos++;
        }
    }

    return dicPos;
}
