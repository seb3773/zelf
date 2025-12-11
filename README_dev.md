# Guide Développeur zELF (Complet)

Ce document constitue la référence exhaustive pour le développement, la maintenance et la compréhension approfondie de l'architecture de zELF. Il synthétise les informations techniques des différents guides (`DEVELOPER_GUIDE.md`, `STUB_LOADER_INTERNALS.md`, `Predictor_Integration_and_Guide.md`).

---

## 1. Architecture Globale

zELF est un packer modulaire conçu pour Linux x86_64. Son architecture sépare distinctement la logique de packing (espace utilisateur standard) de la logique de chargement (stub nostdlib injecté).

```mermaid
graph TB
    subgraph "Packer (Build Time)"
        CLI[CLI Parser] --> ELF_IN[ELF Reader/Validator]
        ELF_IN --> STRIP[Super-Strip]
        STRIP --> FILTER[Filter Selection (ML/Manual)]
        FILTER --> COMPRESS[Compression Dispatch]
        COMPRESS --> STUB_SEL[Stub Selection]
        STUB_SEL --> BUILD[ELF Builder]
        BUILD --> OUT[Packed ELF Output]
    end

    subgraph "Loader (Runtime)"
        ENTRY[Check OS/Arch] --> SYSCALLS[Direct Syscalls]
        SYSCALLS --> DECOMP[Decompression]
        DECOMP --> UNFILTER[Reverse Filter (BCJ/Kanzi)]
        UNFILTER --> MAP[Memory Mapping]
        MAP --> RELOC[PIE Relocation]
        RELOC --> HANDOFF[Stack/Reg Restore & Jump]
    end
```

### 1.1 Flux de Données (Packing)
1.  **Input**: Binaire ELF64 (ET_EXEC ou ET_DYN).
2.  **Sanitization**: Parsing des headers, validation architecture, nettoyage des sections inutiles (strip).
3.  **Filtrage**: Transformation des opcodes x86 (CALL/JMP) pour améliorer la compression (BCJ ou KanziEXE). Le choix est guidé par des modèles de Machine Learning (arbres de décision) ou forcé par l'utilisateur.
4.  **Compression**: Le flux filtré est compressé avec l'un des 16 codecs supportés.
5.  **Encapsulation**:
    *   Un stub (loader) approprié est sélectionné (statique/dynamique, codec spécifique).
    *   Les métadonnées (taille originale, offsets, paramètres) sont injectées.
    *   Le tout est écrit dans un nouvel ELF exécutable.

---

## 2. Organisation du Code

```
ELFZ/
├── src/
│   ├── packer/          # Cœur du packer (Host)
│   │   ├── zelf_packer.c        # Main, CLI, orchestration
│   │   ├── elf_utils.c          # Parsing/ écriture ELF
│   │   ├── compression.c        # Façade unique vers les 16 codecs
│   │   ├── filter_selection.c   # Logique ML et application filtres
│   │   ├── stub_selection.c     # Choix du blob binaire stub
│   │   ├── elf_builder.c        # Assemblage final
│   │   ├── depacker.c           # Logique --unpack
│   │   └── *_predict_dt.h       # Modèles ML (C header-only)
│   │
│   ├── stub/            # Runtime Loader (Target nostdlib)
│   │   ├── start.S              # Entry point ASM (_start)
│   │   ├── stub_static.c        # Logique binaires statiques
│   │   ├── stub_dynamic.c       # Logique binaires PIE/Dynamic
│   │   ├── stub_syscalls.h      # Wrappers syscalls inline ASM
│   │   ├── stub_loader.h        # Chargement ld.so (pour dyn)
│   │   ├── stub_reloc.h         # Relocalisation runtime
│   │   └── codec_select.h       # Dispatch décompresseurs
│   │
│   ├── compressors/     # Sources C des compresseurs (Host side)
│   ├── decompressors/   # Sources C/ASM des décompresseurs (Stub side)
│   ├── filters/         # Implémentations BCJ / KanziEXE
│   └── tools/           # Outils d'analyse (elfz_probe, etc.)
│
├── tools/               # Scripts de build, packaging, ML
│   ├── collect_codec_deep.sh    # Collecte datasets
│   ├── gen_stubs_mk.py          # Génération Makefile stubs
│   └── ml/                      # Scripts d'entraînement Python
│
├── doc/                 # Documentation détaillée
└── Makefile             # Build system principal
```

---

## 3. Internals du Stub/Loader

Le stub est la partie la plus critique et complexe. Il s'agit d'un exécutable minimaliste injecté au début du fichier packé.

### 3.1 Contraintes Strictes (Nostdlib)
Le code du stub (`src/stub/`) est compilé avec `-nostdlib -fno-stack-protector -fomit-frame-pointer`.
*   **Aucune libc** : Pas de `printf`, `malloc`, `memcpy`. Tout doit être implémenté via syscalls ou fonctions inline (`stub_utils.h`).
*   **Position Independent Code (PIC)** : Le stub peut être chargé n'importe où en mémoire. Tout adressage est relatif à RIP.
*   **Pas de globales mutables** : La section `.data` est évitée ou fusionnée. Les états sont sur la stack.

### 3.2 Syscalls Directs (`stub_syscalls.h`)
Les appels système Linux sont invoqués via inline assembly (`asm volatile "syscall"`).
*   Principaux syscalls : `mmap` (alloc), `munmap` (free), `open`, `close`, `read`, `write` (logs), `mprotect` (perms), `exit`.
*   Spéciaux : `memfd_create` + `execveat` (pour certains modes fast-path ou compatibilité).

### 3.3 Cycle de Vie au Runtime
1.  **_start (ASM)** : Sauvegarde tous les registres (contexte initial du kernel) sur la stack. Appelle `loader_main`.
2.  **Localisation** : Trouve le payload compressé.
    *   *Statique* : Via adresses absolues patchées dans `.rodata`.
    *   *PIE* : En scannant sa propre mémoire (VMA) à la recherche du marker magique (ex: `zELFl4` pour LZ4).
3.  **Mapping** : Alloue une zone mémoire anonyme (`mmap`) pour le binaire décompressé.
4.  **Décompression** : Appelle le décompresseur spécifique (code inline inclus via `codec_select.h`).
5.  **Unfilter** : Inverse la transformation BCJ ou KanziEXE.
6.  **Loading ELF** : Parse les Program Headers du binaire restauré et mappe les segments `PT_LOAD` aux bonnes adresses virtuelles.
7.  **Relocalisation (PIE uniquement)** :
    *   Applique les corrections `DT_RELA`.
    *   Patch la GOT/PLT si nécessaire.
    *   Charge l'interpréteur dynamique (`ld-linux-x86-64.so`) si le binaire original est dynamique.
8.  **Handoff** : Restaure la stack, met à jour l'Auxiliary Vector (AUXV) pour tromper la libc/ld.so (lui faire croire qu'elle lance le binaire original), et saute à l'entry point original.

---

## 4. Système de Compression et Codecs

zELF supporte 16 algorithmes. L'ajout d'un codec est standardisé.

### 4.1 Liste des Codecs
LZ4 (défaut/rapide), LZMA (ratio max/lent), ZSTD (équilibré), Apultra, ZX7B, ZX0, BriefLZ, Exomizer, PowerPacker, Snappy, Doboz, QuickLZ, LZAV, Shrinkler, StoneCracker, LZSA2.

### 4.2 Ajouter un Codec
1.  **Sources** : Placer le compresseur dans `src/compressors/<name>/` et le décompresseur (compatible nostdlib !) dans `src/decompressors/<name>/`.
2.  **Packer** : Ajouter le cas dans `src/packer/compression.c`.
3.  **Stub** :
    *   Ajouter le marker dans `src/stub/codec_marker.h`.
    *   Ajouter l'include conditionnel dans `src/stub/codec_select.h`.
4.  **Build** : Ajouter le nom dans la liste `CODECS` de `tools/gen_stubs_mk.py`.

---

## 5. Extensions et Personnalisation

### 5.1 Ajout d'un nouveau codec (Guide pas-à-pas)

#### 5.1.1 Fichiers à créer/modifier

| Étape | Fichier | Action |
|-------|---------|--------|
| 1 | `src/compressors/mycodec/` | Créer répertoire + source compresseur |
| 2 | `src/decompressors/mycodec/` | Créer répertoire + source décompresseur |
| 3 | `src/packer/compression.c` | Ajouter case dans dispatch |
| 4 | `src/stub/codec_select.h` | Ajouter `#ifdef CODEC_MYCODEC` |
| 5 | `src/stub/codec_marker.h` | Ajouter marker 6 bytes |
| 6 | `tools/gen_stubs_mk.py` | Ajouter codec à la liste |
| 7 | `Makefile` | Ajouter règles de compilation |

#### 5.1.2 Exemple: ajout d'un codec "MYCODEC"

**Étape 1: Compresseur** (`src/compressors/mycodec/mycodec.c`)
```c
#include <stdint.h>
#include <stddef.h>

// Prototype requis par compression.c
int mycodec_compress(const unsigned char *src, size_t src_len,
                     unsigned char *dst, size_t dst_cap) {
    // Implémenter compression
    // Retourner taille compressée ou -1 si erreur
    return compressed_size;
}
```

**Étape 2: Décompresseur** (`src/decompressors/mycodec/mycodec_decompress.c`)
```c
// IMPORTANT: Code nostdlib, pas de libc
// Utiliser uniquement types primitifs et inline

static inline int mycodec_decompress(const char *src, char *dst,
                                     int src_len, int dst_cap) {
    // Implémenter décompression
    // Retourner taille décompressée ou -1 si erreur
    return decompressed_size;
}
```

**Étape 3: Intégration packer** (`src/packer/compression.c`)
```c
// Ajouter include
#include "../compressors/mycodec/mycodec.c"

// Dans compress_data_with_codec():
} else if (strcmp(codec, "mycodec") == 0) {
    int csize = mycodec_compress(input, input_size, *output, input_size);
    if (csize > 0) *output_size = csize;
    return csize > 0 ? 0 : -1;
}
```

**Étape 4: Intégration stub** (`src/stub/codec_select.h`)
```c
#ifdef CODEC_MYCODEC
#include "../decompressors/mycodec/mycodec_decompress.c"
static inline int lz4_decompress(const char *src, char *dst,
                                 int src_len, int dst_cap) {
    return mycodec_decompress(src, dst, src_len, dst_cap);
}
#endif
```

**Étape 5: Marker** (`src/stub/codec_marker.h`)
```c
#ifdef CODEC_MYCODEC
static const char COMP_MARKER[] = "zELFmc";  // 6 bytes uniques
#define COMP_MARKER_LEN 6
#endif
```

**Étape 6: Génération stubs** (`tools/gen_stubs_mk.py`)
```python
CODECS = ["lz4", "apultra", ..., "mycodec"]
```

**Étape 7: Makefile**
```makefile
# Ajouter à CODEC_FLAGS
CODEC_MYCODEC_FLAGS = -DCODEC_MYCODEC

# Règle de compilation (générée par gen_stubs_mk.py)
```

**Test du nouveau codec**:
```bash
make clean && make
./zelf -mycodec /usr/bin/ls -o ls_packed
./ls_packed --version
```

### 5.2 Entraînement et intégration d'un prédicteur

Pour améliorer le ratio, zELF transforme le code machine avant compression. zELF utilise des arbres de décision (Random Forest distillés) pour prédire quel filtre (BCJ vs Kanzi) donnera le meilleur ratio pour un couple (Binaire, Codec) donné.

#### 5.2.1 Collecte des données
```bash
# Collecter métriques sur corpus de binaires
./tools/collect_codec_deep.sh --codec mycodec --out stats/mycodec_deep.csv
# Optionnel: limiter à N binaires, scanner autre répertoire
./tools/collect_codec_deep.sh --codec mycodec --dir /opt/bin --out stats/mycodec_deep.csv --limit 300
```

**Format CSV attendu**:
```csv
file,size,bcj_ratio,kanzi_ratio,text_entropy,e8_cnt,e9_cnt,...
/usr/bin/ls,142144,0.432,0.445,6.21,1523,89,...
```

#### 5.2.2 Entraînement du modèle (Python)
```python
import pandas as pd
from sklearn.tree import DecisionTreeClassifier, export_text

df = pd.read_csv("stats/mycodec_deep.csv")

# Target: 1 si BCJ meilleur, 0 si KanziEXE meilleur
df['target'] = (df['bcj_ratio'] < df['kanzi_ratio']).astype(int)

features = ['text_entropy', 'e8_cnt', 'e9_cnt', 'eb_cnt', 
            'jcc32_cnt', 'riprel_est', 'nop_cnt']
X = df[features]
y = df['target']

clf = DecisionTreeClassifier(max_depth=6, min_samples_leaf=20)
clf.fit(X, y)

# Export arbre de décision
print(export_text(clf, feature_names=features))
```

#### 5.2.3 Génération du header C
Convertir l'arbre en `src/packer/mycodec_predict_dt.h`:
```c
// Auto-generated decision tree for mycodec filter selection
static inline int mycodec_predict_bcj(
    double text_entropy, int e8_cnt, int e9_cnt,
    int eb_cnt, int jcc32_cnt, int riprel_est, int nop_cnt) {
    
    if (text_entropy <= 5.8) {
        if (e8_cnt <= 500) return 0;  // KanziEXE
        else return 1;                 // BCJ
    } else {
        if (riprel_est <= 200) return 0;
        else return 1;
    }
}
```

#### 5.2.4 Intégration dans filter_selection.c
```c
#include "mycodec_predict_dt.h"

exe_filter_t decide_exe_filter_auto_mycodec(...) {
    // Extraire features
    double text_ent = compute_entropy(text_seg, text_size);
    int e8 = count_opcode(text_seg, text_size, 0xE8);
    // ...
    
    int use_bcj = mycodec_predict_bcj(text_ent, e8, ...);
    return use_bcj ? EXE_FILTER_BCJ : EXE_FILTER_KANZIEXE;
}
```

---

## 6. Format de Fichier zELF

Un binaire packé par zELF respecte la structure ELF valide, enrichie de structures spécifiques.

### 6.1 Structure ELF packé

```text
┌──────────────────────────────────┐
│ Elf64_Ehdr                       │  64 bytes
├──────────────────────────────────┤
│ Elf64_Phdr[0] (PT_LOAD stub)     │  56 bytes
│ Elf64_Phdr[1] (PT_INTERP opt)    │  56 bytes (si dynamique)
├──────────────────────────────────┤
│ .interp (si dynamique)           │  ~28 bytes
├──────────────────────────────────┤
│ Stub code                        │  1.5KB - 22KB selon codec
├──────────────────────────────────┤
│ .rodata.elfz_params              │  32 ou 48 bytes (voir ci-dessous)
├──────────────────────────────────┤
│ Compressed data block            │  Variable (voir ci-dessous)
└──────────────────────────────────┘
```

#### Bloc `.rodata.elfz_params`

**Sans password (32 bytes):**
| Champ | Taille | Description |
|-------|--------|-------------|
| Magic | 8 bytes | `+zELF-PR` |
| Version | 8 bytes | `1` |
| virtual_start | 8 bytes | Adresse virtuelle stub |
| packed_data_vaddr | 8 bytes | Adresse virtuelle données |

**Avec password (48 bytes):**
| Champ | Taille | Description |
|-------|--------|-------------|
| Magic | 8 bytes | `+zELF-PR` |
| Version | 8 bytes | `2` |
| virtual_start | 8 bytes | Adresse virtuelle stub |
| packed_data_vaddr | 8 bytes | Adresse virtuelle données |
| salt | 8 bytes | Salt aléatoire 64-bit |
| pwd_obfhash | 8 bytes | Hash FNV-1a obfusqué |

#### Bloc de données compressées

**Layout standard** (KanziEXE ou NONE filter, sauf ZSTD):
| Champ | Taille |
|-------|--------|
| COMP_MARKER | 6 bytes |
| original_size | 8 bytes |
| entry_offset | 8 bytes |
| compressed_size | 4 bytes |
| filtered_size | 4 bytes |
| compressed_stream | N bytes |

**Layout legacy** (ZSTD, ou BCJ filter avec tout codec):
| Champ | Taille |
|-------|--------|
| COMP_MARKER | 6 bytes |
| original_size | 8 bytes |
| entry_offset | 8 bytes |
| compressed_size | 4 bytes |
| compressed_stream | N bytes |

*Note: Le layout legacy omet `filtered_size` car avec BCJ, `filtered_size == original_size`.*

### 6.2 Markers par codec

| Codec | Marker | Hex |
|-------|--------|-----|
| LZ4 | `zELFl4` | 7A 45 4C 46 6C 34 |
| Apultra | `zELFap` | 7A 45 4C 46 61 70 |
| ZX7B | `zELFzx` | 7A 45 4C 46 7A 78 |
| ZX0 | `zELFz0` | 7A 45 4C 46 7A 30 |
| BriefLZ | `zELFbz` | 7A 45 4C 46 62 7A |
| Exomizer | `zELFex` | 7A 45 4C 46 65 78 |
| PowerPacker | `zELFpp` | 7A 45 4C 46 70 70 |
| Snappy | `zELFsn` | 7A 45 4C 46 73 6E |
| Doboz | `zELFdz` | 7A 45 4C 46 64 7A |
| QuickLZ | `zELFqz` | 7A 45 4C 46 71 7A |
| LZAV | `zELFlv` | 7A 45 4C 46 6C 76 |
| LZMA | `zELFla` | 7A 45 4C 46 6C 61 |
| ZSTD | `zELFzd` | 7A 45 4C 46 7A 64 |
| Shrinkler | `zELFsh` | 7A 45 4C 46 73 68 |
| StoneCracker | `zELFsc` | 7A 45 4C 46 73 63 |
| LZSA2 | `zELFls` | 7A 45 4C 46 6C 73 |

---

## 7. Build System

Le build est piloté par un `Makefile` racine et un générateur Python pour les stubs.

### Commandes Principales
*   `make` : Build tout (packer dynamique, stubs, outils).
*   `make STATIC=1` : Build le packer en statique (sans dépendance runtime glibc).
*   `make stubs` : Recompile uniquement les stubs (utilise `tools/gen_stubs_mk.py`).
*   `make tools` : Compile `elfz_probe` et `filtered_probe`.
*   `make package` / `make deb` / `make tar` : Crée les paquets de distribution.

### Dépendances de Build
*   **Compilateurs** : `gcc`, `g++`.
*   **Outils** : `make`, `nasm` (pour certains décompresseurs asm), `python3`.
*   **Librairies (Dev)** : `liblz4-dev`, `libzstd-dev`, `zlib1g-dev` (pour le packer host).

---

## 8. Debugging et Maintenance

### Debugging du Stub
Puisque `gdb` est difficile à utiliser sur un stub qui se remplace lui-même en mémoire :
1.  **Logs** : Activer les `z_syscall_write` commentés dans le code du stub pour tracer l'exécution sur stdout/stderr.
2.  **Exit Codes** : Le stub retourne des codes spécifiques en cas d'erreur (voir `stub_syscalls.h` ou doc interne) :
    *   `1-3` : Erreurs système (mmap, read).
    *   `4` : Marker non trouvé (corruption ou mauvais parsing).
    *   `5` : Erreur décompression.
    *   `6` : Format ELF invalide après décompression.
3.  **Strace** : `strace -f ./packed_bin` est l'outil le plus efficace pour voir où le stub échoue (mmap fail, segfault sur jump, etc.).

### Tests et CI

#### Scripts de test existants

| Script | Fonction |
|--------|----------|
| `build_and_test.sh` | Pack/exec `/usr/bin/ls` + binaire statique |
| `test_bin_unpack.sh` | Round-trip pack → unpack → compare |
| `SPEEDTEST=1 ./build_and_test.sh` | Benchmark compression/décompression |

#### Lancer les tests

```bash
# Test basique
./build_and_test.sh -lz4

# Test tous les codecs
for c in lz4 apultra zx7b zstd lzma; do
    echo "=== $c ===" && ./build_and_test.sh -$c
done

# Test avec mode verbose (affiche tout)
./build_and_test.sh -lz4 -full
```

#### Ajouter un test pour nouveau codec
Dans `build_and_test.sh`, ajouter la ligne suivante :
```bash
test_codec "mycodec" "/usr/bin/ls"
```

---

## 9. Conventions de Développement

*   **Langage** : C99 pour le packer, C + Inline ASM pour le stub.
*   **Style** : Indentation 4 espaces, accolades ouvrantes sur la même ligne.
*   **Commentaires** : En anglais technique.
*   **Performance** :
    *   Privilégier `static inline` dans le stub.
    *   Éviter les allocations dynamiques dans le packer si possible (utiliser la stack ou des buffers réutilisables).
*   **Modularité** : Toute nouvelle fonctionnalité (codec, filtre) doit être isolée dans son propre module et intégrée via les interfaces génériques (`compression.c`, `filter_selection.c`).
