#ifndef CODEC_MARKER_H
#define CODEC_MARKER_H

#ifdef CODEC_ZX7B
#define COMP_MARKER "zELFzx"
#elif defined(CODEC_ZX0)
#define COMP_MARKER "zELFz0"
#elif defined(CODEC_ZSTD)
#define COMP_MARKER "zELFzd"
#elif defined(CODEC_EXO)
#define COMP_MARKER "zELFex"
#elif defined(CODEC_SNAPPY)
#define COMP_MARKER "zELFsn"
#elif defined(CODEC_DOBOZ)
#define COMP_MARKER "zELFdz"
#elif defined(CODEC_QLZ)
#define COMP_MARKER "zELFqz"
#elif defined(CODEC_LZAV)
#define COMP_MARKER "zELFlv"
#elif defined(CODEC_LZMA)
#define COMP_MARKER "zELFla"
#elif defined(CODEC_PP)
#define COMP_MARKER "zELFpp"
#elif defined(CODEC_APULTRA)
#define COMP_MARKER "zELFap"
#elif defined(CODEC_SHRINKLER)
#define COMP_MARKER "zELFsh"
#elif defined(CODEC_BRIEFLZ)
#define COMP_MARKER "zELFbz"
#elif defined(CODEC_SC)
#define COMP_MARKER "zELFsc"
#elif defined(CODEC_LZSA)
#define COMP_MARKER "zELFls"
#else
#define COMP_MARKER "zELFl4"
#endif
#define COMP_MARKER_LEN 6

#endif /* CODEC_MARKER_H */
