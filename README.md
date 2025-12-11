# zELF — Packer ELF x86_64

zELF est un packer/modificateur d’exécutables ELF 64 bits pour Linux x86_64, inspiré d'UPX, mais avec une structure modulaire : 16 codecs de compression, la possibilité d'integrer vos propres codecs, deux filtres exécutables (BCJ, KanziEXE) et une sélection automatique par modèles ML. Il gère les binaires statiques, dynamiques/PIE, et propose un mode archive `.zlf` ainsi qu’un mode `-best` qui explore plusieurs combinaisons pour maximiser le ratio.

## Fonctionnalités clés
- Compression multi-codecs : LZ4, LZMA, ZSTD, Apultra, ZX7B, ZX0, BriefLZ, Exomizer, PowerPacker, Snappy, Doboz, QuickLZ, LZAV, Shrinkler, StoneCracker, LZSA2.
- Filtres EXE : BCJ et KanziEXE, sélection automatique via arbres de décision entraînés (mode par défaut `auto`).
- Support ELF : binaires statiques et dynamiques/PIE, validation stricte des en-têtes et segments.
- Mode `-best` : choisit 3 codecs prédits selon la taille du binaire, teste filtres `bcj → kanzi → none`, conserve le meilleur.
- Mode archive `.zlf` et dépaquetage (`--archive`, `--unpack`).
- Option mot de passe (hash FNV-1a obfusqué dans le stub).
- Stubs nostdlib compacts (1.5–22 KB), repackés dans le binaire final.
- Outils d’analyse (elfz_probe, filtered_probe) dans `build/tools/`.

## Plateforme et limites
- Cible uniquement Linux x86_64, ELF64. Les ELF 32 bits ne sont pas supportés.
- Please note that zELF is not designed for code obfuscation or concealment. All source code is openly available, and even the more exotic compression format can be integrated and parsed by any analysis tool thanks to the provided sources. The goal of this packer is binary size reduction and efficient runtime unpacking—not evasion of antivirus software or distribution of malicious code. It is intended for legitimate use cases such as embedded systems, demos, general binary optimization or educational purposes like studying ELF structure and advanced packing techniques.

## Build
Vous pouvez utiliser le menu interactif pour configurer et lancer le build :
```bash
make menu
```
Ou utiliser les commandes manuelles ci-dessous :

### Installation des dépendances
Sur Debian/Ubuntu :
```bash
make install_dependencies
```
Pour d’autres distributions, installez l’équivalent de : gcc/g++/make/nasm/pkg-config, zlib-dev, lz4-dev, zstd-dev, python3/pip, dpkg-deb, dialog/whiptail. Pour les builds statiques, les libs statiques (libstdc++/glibc) doivent être disponibles sur le système.

### Commandes de build
```bash
# Build complet (stubs, outils, packer)
make
# Build statique
make STATIC=1
# Paralléliser la génération des stubs (par défaut 2)
make STUBS_J=4 stubs
# Outils seuls
make tools
# Packages
make deb        # génère un .deb, suffixe _static si STATIC=1
make tar        # génère un .tar.gz, suffixe _static si STATIC=1
```
Le binaire packer final est `build/zelf`.

## Usage rapide
```bash
./build/zelf [options] <binaire_elf>
```
Principales options utilisateur :
- Sélection codec : `-lz4` (défaut), `-lzma`, `-zstd`, `-apultra`, `-zx7b`, `-zx0`, `-blz`, `-exo`, `-pp`, `-snappy`, `-doboz`, `-qlz`, `-lzav`, `-shr` (shrinkler), `-stcr` (stonecracker), `-lzsa`.
- Filtre EXE : `--exe-filter=auto|bcj|kanziexe|none` (ignoré en mode `-best`, ordre forcé bcj→kanzi→none).
- Mode `-best` : explore 3 codecs prédits + filtres, garde le meilleur.
- Strip : `--no-strip` pour désactiver le super-strip avant compression.
- Mot de passe : `--password` (demande le mot de passe, sel aléatoire).
- Sortie : `--output <fichier|répertoire>`, `--nobackup` pour ne pas créer `.bak` lors de l’overwrite.
- Archives : `--archive` (crée un `.zlf`), `--unpack` (dépaquette ELF packé ou archive `.zlf`).
- Affichage : `--verbose`, `--no-progress`, `--no-colors`.

### Exemples
```bash
# Pack simple (lz4 par défaut), écrase l'original après backup .bak à côté
./build/zelf /usr/bin/ls

# Pack avec ZSTD + filtre BCJ explicite
./build/zelf -zstd --exe-filter=bcj /usr/bin/ls --output /tmp/ls_packed

# Mode -best (essais multi-codecs/filtre), sortie explicite
./build/zelf -best /usr/bin/true --output /tmp/true_best

# Archive d'un fichier quelconque
./build/zelf -zstd --archive some_data --output archive.zlf

# Dépaquetage d'une archive .zlf
./build/zelf --unpack archive.zlf --output restored_data

# Restauration d'un binaire packé (unpacking in-place ou vers nouveau fichier)
./build/zelf --unpack /tmp/ls_packed --output /tmp/ls_original
```

## Mode `-best` (détail)
- Sélectionne 3 codecs selon la taille (table intégrée).
- Teste pour chaque codec les filtres `bcj`, `kanzi`, `none` (au total 9 essais).
- Garde uniquement le binaire le plus petit.
- `--exe-filter` est ignoré dans ce mode.

## Documentation développeurs
Pour une documentation exhaustive sur l'architecture et le développement :
- **[README_dev.md](README_dev.md)** : Guide développeur complet et exhaustif (Architecture, Codecs, ML, Stubs).

Autres documents de référence :
- `doc/ZELF_ANALYSIS_REPORT.md` : Rapport d'analyse technique (Architecture & Analyse).
- `doc/STUB_LOADER_INTERNALS.md` : Détails internes du stub/loader (Bas niveau).
- `doc/Predictor_Integration_and_Guide.md` : Pipeline ML et prédicteurs (ML).
- `doc/ANALYSE_SYSTEME_STATS_ML.md` : Analyses statistiques.

## Licence
Licence principale : TFYW (The Fuck You Want), sous réserve des licences des composants intégrés (voir sources des codecs/filtres).
