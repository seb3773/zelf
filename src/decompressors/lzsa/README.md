# LZSA2 Compression Library

Bibliothèque de compression/décompression LZSA2 optimisée, avec accent sur la minimisation de la taille du décompresseur.

## Performances

### Compression (test sur /usr/bin/scalar - 2.1 MB)
- **Ratio** : 48.55% (1 067 898 octets compressés)
- **Algorithme** : LZSA2 avec option `--prefer-ratio` (compression maximale)

### Tailles des bibliothèques
- **Bibliothèque de compression** : 87 KB (liblzsa2_compress.a)
- **Bibliothèque de décompression** : 15 KB (liblzsa2_decompress.a) - optimisée taille
- **Décompresseur ASM x86-32** : 237 octets (code pur)
- **Décompresseur ASM Z80** : 67 octets (le plus compact !)

## Structure

```
lzsa_lib/
├── compressor/          # Sources du compresseur LZSA2
│   └── libdivsufsort/   # Suffix array pour matching optimal
├── decompressor/        # Sources du décompresseur LZSA2
│   ├── asm_decompressors/  # Décompresseurs assembleur
│   │   ├── x86/         # x86 32-bit (237 octets)
│   │   ├── z80/         # Z80 (67 octets !)
│   │   ├── 6502/        # 6502 (Apple II, C64, NES...)
│   │   └── 8088/        # 8088/8086 (IBM PC)
│   └── libdivsufsort/   # Headers nécessaires
├── libs/
│   ├── compression/     # Bibliothèque de compression statique
│   │   ├── lzsa2_compress.h
│   │   ├── lzsa2_compress.c
│   │   └── liblzsa2_compress.a (87 KB)
│   └── decompression/   # Bibliothèque de décompression (optimisée taille)
│       ├── lzsa2_decompress.h
│       ├── lzsa2_decompress.c
│       └── liblzsa2_decompress.a (15 KB)
├── tests/               # Programmes de test
│   ├── test_compress    # Test de compression
│   └── test_decompress  # Test de décompression
└── docs/                # Documentation du format
```

## Compilation

```bash
make                    # Compile tout (libs + tests)
make libs              # Compile uniquement les bibliothèques
make tests             # Compile uniquement les tests
make clean             # Nettoie les fichiers générés
```

## Utilisation des bibliothèques

### Compression (lzsa2_compress.h)

```c
#include "lzsa2_compress.h"

// Obtenir la taille max du buffer compressé
size_t max_size = lzsa2_compress_get_max_size(input_size);

// Compresser en mode maximum (LZSA_FLAG_FAVOR_RATIO)
int compressed_size = lzsa2_compress(input_data, output_buffer,
                                     input_size, max_output_size,
                                     LZSA_FLAG_FAVOR_RATIO, 3);
```

**Flags de compression disponibles** :
- `LZSA_FLAG_FAVOR_RATIO` : Compression maximale (défaut)
- `LZSA_FLAG_RAW_BLOCK` : Format bloc brut (max 64KB)
- `LZSA_FLAG_RAW_BACKWARD` : Compression backward (avec RAW_BLOCK)

### Décompression (lzsa2_decompress.h)

```c
#include "lzsa2_decompress.h"

// Obtenir la taille max décompressée
int max_size = lzsa2_decompress_get_max_size(compressed_data, compressed_size);

// Décompresser
int decompressed_size = lzsa2_decompress(compressed_data, output_buffer,
                                         compressed_size, max_output_size, 0);
```

**Note** : La bibliothèque de décompression est optimisée pour la taille avec :
- Flag `-DNDEBUG` pour éliminer les assertions
- Pas de messages de debug (seulement erreurs critiques)
- Code minimal et épuré

## Programmes de test

```bash
# Compiler les tests
cd tests && make

# Tester la compression (mode maximum)
./test_compress input.bin output.lzsa2

# Tester la décompression
./test_decompress output.lzsa2 restored.bin

# Vérifier l'intégrité
md5sum input.bin restored.bin
```

## Compilation d'un programme avec les bibliothèques

### Programme de compression
```bash
gcc -o my_compressor my_compressor.c \
    -I lzsa_lib/libs/compression \
    -L lzsa_lib/libs/compression \
    -llzsa2_compress
```

### Programme de décompression
```bash
gcc -o my_decompressor my_decompressor.c \
    -I lzsa_lib/libs/decompression \
    -L lzsa_lib/libs/decompression \
    -llzsa2_decompress
```

## Format LZSA2

LZSA2 est conçu pour être extrêmement rapide à décompresser sur systèmes 8-bit :

### Caractéristiques
- **Encodage par nibbles** : Pas de bit-packing lent, utilise des nibbles (4 bits)
- **Rep-matches** : Réutilisation du dernier offset pour meilleure compression
- **Offsets variables** : 5-bit, 9-bit, 13-bit, 16-bit selon la distance
- **Minimum match** : 2 octets (vs 4 pour LZ4, 3 pour LZSA1)
- **Format stream** : Header + frames de 64KB max + footer EOD

### Format Stream
```
[Header 3 bytes]
[Frame 1: 3 bytes length + data]
[Frame 2: 3 bytes length + data]
...
[Footer: 0x00 0x00 0x00]
```

### Comparaison avec autres algos

| Algorithme | Ratio    | Vitesse décomp | Taille décomp |
|------------|----------|----------------|---------------|
| **LZSA2**  | **48%**  | **75% de LZ4** | **237 bytes** |
| ZX7        | 53%      | 48% de LZ4     | ~180 bytes    |
| LZSA1      | 57%      | 90% de LZ4     | ~200 bytes    |
| LZ4        | 61%      | 100% (ref)     | ~300 bytes    |

**LZSA2 offre le meilleur compromis entre compression et vitesse de décompression.**

## Documentation

Voir `docs/` pour :
- `BlockFormat_LZSA2.md` : Spécification complète du format bloc LZSA2
- `StreamFormat.md` : Format de stream avec header/frames
- `README.md` : Documentation originale LZSA

## Décompresseurs assembleur

Le répertoire `decompressor/asm_decompressors/` contient des décompresseurs optimisés pour :

- **x86** : Version taille (237 bytes) et version vitesse
- **Z80** : Version taille (67 bytes !) et version vitesse
- **6502** : Pour Apple II, Commodore 64, NES...
- **8088** : Pour IBM PC et compatibles

Ces décompresseurs sont prêts à l'emploi pour projets retro-computing, demos, jeux 8-bit, etc.

## Licence

- **Code LZSA** : Zlib License
- **Suffix array (libdivsufsort)** : CC0 License

## Crédits

- **Emmanuel Marty** : Auteur original de LZSA
- **introspec & uniabis** : Décompresseurs Z80 optimisés
- **Multiples contributeurs** : Décompresseurs pour autres architectures

Voir le README original dans `docs/` pour la liste complète.
