#include <stdint.h>
#include <stddef.h>
#include "stub_vars.h"
#include "codec_select.h"
#include "codec_marker.h"
#include "bcj_x86_min.h"
#if defined(CODEC_ZSTD)
#include "zstd_minidec.h"
#endif

// Noreturn helper for static binaries: restore original stack and jump to e_entry
// (unused in dynamic stub; keep commented for reference)
// void static_final_transfer(void *entry, void *orig_sp);

#include "stub_defs.h"
#include "stub_utils.h"

#define simple_hex_string(a,b) ((void)0)
#define simple_decimal_string(a,b) ((void)0)

// Minimal params block for dynamic stub
#if defined(ELFZ_STUB_PASSWORD)
__attribute__((used, section(".rodata.elfz_params")))
static const unsigned char elfz_params_block[] = {
    '+','z','E','L','F','-','P','R',
    2,0,0,0,0,0,0,0,                  // version = 2
    0,0,0,0,0,0,0,0,                  // virtual_start (unused for dynamic)
    0,0,0,0,0,0,0,0,                  // packed_data_vaddr (unused for dynamic)
    0,0,0,0,0,0,0,0,                  // salt (to be patched)
    0,0,0,0,0,0,0,0                   // pwd_obfhash (to be patched)
};
#else
__attribute__((used, section(".rodata.elfz_params")))
static const unsigned char elfz_params_block[] = {
    '+','z','E','L','F','-','P','R',
    1,0,0,0,0,0,0,0,                  // version = 1 (flags may be ORed by packer)
    0,0,0,0,0,0,0,0,                  // virtual_start (unused for dynamic)
    0,0,0,0,0,0,0,0,                  // packed_data_vaddr (unused for dynamic)
    0,0,0,0,0,0,0,0                   // reserved
};
#endif

#include "stub_syscalls.h"
#include "stub_elf.h"
#include "stub_reloc.h"
#include "stub_loader.h"

// Bind to tiny ASM implementations in mini_mem.S
extern void* simple_memcpy(void* dst, const void* src, size_t n);
extern void* simple_memset(void* dst, int c, size_t n);

// Corrected verification helper
static int verify_entry_point_with_real_mapping(uint64_t entry_point) {
    // 1. Ensure entry point is reachable
    if (z_syscall_mprotect((void*)(entry_point & ~0xFFF), 4096, PROT_READ | PROT_EXEC) != 0) {
        return 0;
    }
    
    // 2. Read the code (should succeed now)
    unsigned char *entry_code = (unsigned char *)entry_point;
   
    // 3. Check the prologue signature
    if (entry_code[0] == 0x55 && entry_code[1] == 0x48 && entry_code[2] == 0x89 && entry_code[3] == 0xE5) {
        return 1;
    } else if (entry_code[0] == 0x48 && entry_code[1] == 0x83 && entry_code[2] == 0xEC) {
        return 1;
    } else if (entry_code[0] == 0x31 && entry_code[1] == 0xED) {
        return 1;
    } else {
        return 1; // At least it is reachable
    }
}

// Called from ASM to prepare the final sequence.
// Sets up AUXV and returns the address of the AT_NULL value field.
// Not static because assembly references it.
// Parameters 7 and 8: hatch_ptr and saved_rdx are passed explicitly.
uint64_t __attribute__((noinline)) elfz_fold_exact_setup(
    uint64_t entry_point, 
    unsigned long *original_sp, 
    uint64_t ADRU, 
    uint64_t LENU,
    void *elf_base,
    void *interp_base,
    void *hatch_ptr,
    uint64_t saved_rdx) {
    
    // Use explicit parameters (safer than globals)
    void *hatch = hatch_ptr;
    // uint64_t saved_rdx_kernel = saved_rdx; // Unused

    
    // 1. Locate AUXV in the original stack
    unsigned long *auxv_ptr = original_sp;
    auxv_ptr++;  // Skip argc
    while (*auxv_ptr != 0) auxv_ptr++;  // Skip argv
    auxv_ptr++;  // Skip NULL
    while (*auxv_ptr != 0) auxv_ptr++;  // Skip envp
    auxv_ptr++;  // Skip NULL
    
    Elf64_auxv_t *auxv = (Elf64_auxv_t *)auxv_ptr;
    
    // 2. Configure AUXV
    Elf64_Ehdr *main_ehdr = (Elf64_Ehdr *)elf_base;
    Elf64_Phdr *main_ph = (Elf64_Phdr *)((char *)elf_base + main_ehdr->e_phoff);
    uint64_t main_phdr = (uint64_t)elf_base + main_ehdr->e_phoff;
    // Prefer PT_PHDR's p_vaddr when present (more accurate for some large binaries)
    for (int i = 0; i < main_ehdr->e_phnum; i++) {
        if (main_ph[i].p_type == PT_PHDR) {
            main_phdr = (uint64_t)elf_base + main_ph[i].p_vaddr;
            break;
        }
    }
    uint64_t main_entry = (uint64_t)elf_base + main_ehdr->e_entry;
    
    auxv_up(auxv, AT_PHDR, main_phdr);
    auxv_up(auxv, AT_PHNUM, main_ehdr->e_phnum);
    auxv_up(auxv, AT_PHENT, main_ehdr->e_phentsize);
    auxv_up(auxv, AT_ENTRY, main_entry);
    auxv_up(auxv, AT_BASE, (uint64_t)interp_base);
    
    // 3. Store hatch in AT_NULL (use the hatch CODE address)
    //    Parameter may be either the direct CODE address of the hatch,
    //    or the address of the AT_NULL value field (pointer-to-pointer).
    void *hatch_code = hatch_ptr;
    if (hatch_code) {
        // Verify signature directly on hatch_code
        volatile uint8_t *hp = (volatile uint8_t *)hatch_code;
        uint32_t sig = ((uint32_t)hp[0]) | (((uint32_t)hp[1]) << 8) |
                       (((uint32_t)hp[2]) << 16) | (((uint32_t)hp[3]) << 24);
        if (sig != 0xC35A050F) {
            // Try pointer-to-pointer: read stored value
            uint64_t maybe = *(volatile uint64_t *)hatch_code;
            volatile uint8_t *hp2 = (volatile uint8_t *)maybe;
            uint32_t sig2 = ((uint32_t)hp2[0]) | (((uint32_t)hp2[1]) << 8) |
                            (((uint32_t)hp2[2]) << 16) | (((uint32_t)hp2[3]) << 24);
            if (sig2 == 0xC35A050F) {
                hatch_code = (void *)maybe;
            }
        }
    }
    auxv_up(auxv, AT_NULL, (uint64_t)hatch_code);
    
    // 4. Find the ADDRESS of the AT_NULL entry (not its value)
    Elf64_auxv_t *at_null = auxv;
    while (at_null->a_type != AT_NULL) at_null++;
    // uint64_t hatch_from_auxv = at_null->a_un.a_val; // Unused
    
    // CRITICAL: compute the ADDRESS of the AT_NULL entry
    uint64_t at_null_entry_addr = (uint64_t)&at_null->a_un.a_val;

    // Verify there is NO overlap
    if (entry_point >= ADRU && entry_point < ADRU + LENU) {
        z_syscall_exit(99);
    }
    
    uint64_t hatch_addr = (uint64_t)hatch;
    if (hatch_addr >= ADRU && hatch_addr < ADRU + LENU) {
        z_syscall_exit(98);
    }
    
    // Do NOT write globals: the dynamic stub is embedded raw (no GOT/RELA)
    // Keep values in registers/stack; ASM already saved them.
    // RETURN: address of the AT_NULL entry (after backward reordering)
   
    return at_null_entry_addr;
}

extern void _start(void);  // Defined in start.S
extern char _end[];        // Defined by the linker (end of BSS)

// Main entry called from assembly
// May be invoked from start.S or stub_dynamic_full.S (stub_dynamic_entry)
// Define both names for compatibility:
#if defined(__GNUC__)
#  define ELFZ_ALIGN_STACK __attribute__((force_align_arg_pointer))
#else
#  define ELFZ_ALIGN_STACK
#endif
ELFZ_ALIGN_STACK void elfz_main_wrapper(unsigned long *stack_frame);
ELFZ_ALIGN_STACK int stub_dynamic_entry(unsigned long *original_sp);

// Shared implementation
static void stub_main_common(unsigned long *original_sp, uint64_t saved_rdx_kernel) {    
    #ifdef CODEC_SHRINKLER
    // z_syscall_write(1, "[shr] enter\n", 12);
    #endif
    // Optional password gate for dynamic stub; echoed input via /dev/tty or stdin
#if defined(ELFZ_STUB_PASSWORD)
    const unsigned char *pb;
    {
        uint64_t pb_addr;
        __asm__ volatile("lea elfz_params_block(%%rip), %0" : "=r"(pb_addr));
        pb = (const unsigned char *)pb_addr;
    }
    if (!(pb[0]=='+' && pb[1]=='z' && pb[2]=='E' && pb[3]=='L' && pb[4]=='F' && pb[5]=='-' && pb[6]=='P' && pb[7]=='R')) {
        z_syscall_exit(12);
    }
    uint64_t salt = *(const uint64_t *)(pb + 32);
    uint64_t obf  = *(const uint64_t *)(pb + 40);
    uint64_t mask = (salt * 11400714819323198485ull) ^ (salt >> 13);
    uint64_t expected = obf ^ mask;
    static const char pw_prompt[] = "Password:";
    static const char pw_wrong[]  = "Wrong password\n";
    (void)z_syscall_write(1, pw_prompt, (int)sizeof(pw_prompt)-1);
    int fd = (int)z_syscall_open("/dev/tty", 0, 0);
    if (fd < 0) fd = 0; // fallback to stdin
    // Disable echo if tty supports it
    int echo_changed = 0;
    struct z_termios { unsigned int c_iflag, c_oflag, c_cflag, c_lflag; unsigned char c_line; unsigned char c_cc[32]; } tio_old, tio_new;
    if (z_syscall_ioctl(fd, TCGETS, &tio_old) == 0) {
        tio_new = tio_old;
        tio_new.c_lflag &= ~Z_ECHO;
        if (z_syscall_ioctl(fd, TCSETS, &tio_new) == 0) {
            echo_changed = 1;
        }
    }
    char pwbuf[64];
    long n = z_syscall_read(fd, pwbuf, (int)sizeof(pwbuf));
    // Restore echo and move to next line if we disabled it
    if (echo_changed) {
        (void)z_syscall_ioctl(fd, TCSETS, &tio_old);
        (void)z_syscall_write(1, "\n", 1);
    }
    if (fd > 0) z_syscall_close(fd);
    if (n <= 0) z_syscall_exit(1);
    int len = (int)n;
    while (len > 0 && (pwbuf[len-1] == '\n' || pwbuf[len-1] == '\r')) len--;
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < 8; i++) { unsigned char b = (unsigned char)((salt >> (8*i)) & 0xff); h ^= b; h *= 1099511628211ull; }
    for (int i = 0; i < len; i++) { h ^= (unsigned char)pwbuf[i]; h *= 1099511628211ull; }
    if (h != expected) { (void)z_syscall_write(1, pw_wrong, (int)sizeof(pw_wrong)-1); z_syscall_exit(1); }
#endif
    // Unified declarations
    uint64_t entry_point = 0;
    // uint64_t binary_entry_point = 0; // unused in dynamic stub
    // int is_static = 1;  // 1 = static, 0 = dynamic (disabled in dynamic stub)
    void *coherent_base = NULL;  // Base address of the decompressed binary
    uint64_t elf_header_offset = 0;  // Location of the ELF header
    
    // original_sp points directly to argc (no prologue adjustment)
    int argc = *(original_sp);
    char **argv = (char **)(original_sp + 1);
    char **envp = argv + argc + 1;

    // Compute the actual base (ASLR may shift even fixed addresses)
    // Obtain _start address at runtime via RIP-relative
    uint64_t current_start_addr;
    __asm__ volatile(
        "lea _start(%%rip), %0\n"
        : "=r" (current_start_addr)
    );
    // Dynamic stub: no params, force marker-scan path
    uint64_t virtual_start = 0;

    // Compute base offset (0 if no ASLR, non-zero if ASLR active)
    uint64_t aslr_offset = current_start_addr - virtual_start;

    // For PIE (VSTART < 0x10000), locate the marker dynamically
    const char *packed_data = NULL;
    if (virtual_start < 0x10000) {
        unsigned char *search_start;
        unsigned char *search_end;
        // Robust /proc/self/maps scan only for codecs that need it (zstd, density).
#if defined(CODEC_ZSTD) || defined(CODEC_DENSITY)
        {
            // Determine a robust scan window: the whole VMA containing _start
            uint64_t vma_s = 0, vma_e = 0;
            search_start = (unsigned char *)current_start_addr;
            search_end = search_start + 0x28000;  // fallback: 160KB
            if (find_vma_for_addr(current_start_addr, &vma_s, &vma_e) == 0) {
                search_start = (unsigned char *)vma_s;
                search_end = (unsigned char *)vma_e;
            }
        }
#else
        {
            // Legacy window: scan a small fixed range around the mapping.
            search_start = (unsigned char *)(current_start_addr & ~0xFFFULL);
            search_end = search_start + 0x28000; // 160KB
        }
#endif
        
        int found_count = 0;
        // Leave 32-byte margin to avoid SIGSEGV while reading metadata
        for (unsigned char *p = search_start; p < search_end - 32; p++) {
            if (p[0]==COMP_MARKER[0] && p[1]==COMP_MARKER[1] && p[2]==COMP_MARKER[2] && p[3]==COMP_MARKER[3] && p[4]==COMP_MARKER[4] && p[5]==COMP_MARKER[5]) {
                // Validate all metadata to ensure we found the correct marker
                uint64_t actual_size = *(uint64_t *)(p + COMP_MARKER_LEN);
                uint64_t entry_off = *(uint64_t *)(p + COMP_MARKER_LEN + sizeof(size_t));
                uint32_t comp_size = *(uint32_t *)(p + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));                
                // Plausibility checks (wider for large binaries):
                // 1. actual_size between 1KB and 512MB
                // 2. comp_size between 100 bytes and 512MB
                if (actual_size > 1000 && actual_size < 0x20000000 &&
                    comp_size > 100 && comp_size < 0x20000000 &&
                    comp_size < actual_size) {
                    // Bounds guard: ensure we can read marker+metadata without crossing the scan window.
                    // Prevent OOB reads when the candidate is near the end of the mapped range.
                    if (p + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(uint32_t) > search_end) {
                        continue;
                    }
                    // Pointer to start of compressed stream
                    unsigned char *cand_stream = p + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(uint32_t);
                    #ifdef CODEC_ZSTD
                    // New layout may include an extra int (bcj flag) before the compressed stream.
                    // Detect by checking the Zstd magic and adjust cand_stream accordingly.
                    if (!(cand_stream[0]==0x28 && cand_stream[1]==0xB5 && cand_stream[2]==0x2F && cand_stream[3]==0xFD)) {
                        if (cand_stream + 4 <= search_end &&
                            cand_stream[4]==0x28 && cand_stream[5]==0xB5 && cand_stream[6]==0x2F && cand_stream[7]==0xFD) {
                            cand_stream += 4;
                        } else {
                            continue;
                        }
                    }
                    #endif
                    #ifdef CODEC_SHRINKLER
                    // Additional validation for Shrinkler: header must exist and be consistent
                    // Ensure enough room to read the 16-byte fixed header fields
                    if (cand_stream + 16 > search_end) {
                        continue;
                    }
                    if (!(cand_stream[0]=='S' && cand_stream[1]=='h' && cand_stream[2]=='r' && cand_stream[3]=='i')) {
                        continue;
                    }
                    unsigned hdr_rem = ((unsigned)cand_stream[6] << 8) | (unsigned)cand_stream[7];
                    unsigned hdr_sz  = 8u + hdr_rem;
                    if (hdr_sz < 24u) {
                        continue;
                    }
                    // Ensure the declared header fits in the search window (avoid OOB)
                    if (cand_stream + hdr_sz > search_end) {
                        continue;
                    }
                    unsigned payload_sz = ((unsigned)cand_stream[8] << 24) | ((unsigned)cand_stream[9] << 16) | ((unsigned)cand_stream[10] << 8) | (unsigned)cand_stream[11];
                    unsigned uncomp_sz  = ((unsigned)cand_stream[12] << 24) | ((unsigned)cand_stream[13] << 16) | ((unsigned)cand_stream[14] << 8) | (unsigned)cand_stream[15];
                    if (uncomp_sz != (unsigned)actual_size) {
                        continue;
                    }
                    if (payload_sz == 0 || (unsigned)comp_size < hdr_sz + payload_sz) {
                        continue;
                    }
                    #endif
                    // LZAV-specific validation: expected format nibble == 2
                    #ifdef CODEC_LZAV
                    if (((cand_stream[0] >> 4) & 0xF) != 2) {
                        continue;
                    }
                    #endif
    #ifdef CODEC_ZSTD
    // Ensure candidate Zstd frame reports original_size when available
    {
        long long fcs_cand = zstd_peek_content_size((const unsigned char*)cand_stream, (size_t)comp_size);
        if (fcs_cand > 0 && (size_t)fcs_cand != actual_size) {
            continue; // Wrong match; keep scanning
        }
    }
    #endif
                    // Extra validation for SNAPPY:
                    // Check compressed stream varint header reports actual_size
                    // Helps discard false positives when COMP_MARKER appears in .rodata
                    #ifdef CODEC_SNAPPY
                    const unsigned char *cptr = (const unsigned char *)(p + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(uint32_t));
                    size_t ulen_hdr = snappy_get_uncompressed_length(cptr, (size_t)comp_size);
                    if (ulen_hdr != (size_t)actual_size) {
                        continue; // Mauvais match; poursuivre la recherche
                    }
                    #endif
                    #ifdef CODEC_LZMA
                    // Validate 5-byte LZMA props: lc<=8, lp<=4, pb<=4; dict in sane range
                    if ((size_t)comp_size >= 5) {
                        const unsigned char *props = (const unsigned char *)cand_stream;
                        unsigned p0 = props[0];
                        unsigned lc = p0 % 9; p0 /= 9;
                        unsigned lp = p0 % 5; unsigned pb = p0 / 5;
                        unsigned dict = (unsigned)props[1] | ((unsigned)props[2] << 8) | ((unsigned)props[3] << 16) | ((unsigned)props[4] << 24);
                        if (!(lc <= 8 && lp <= 4 && pb <= 4)) {
                            continue;
                        }
                        if (dict < 4096 || dict > (256u << 20)) { // 4 KiB .. 256 MiB
                            continue;
                        }
                    } else {
                        continue;
                    }
                    #endif
                    // C'est le bon marqueur !
                    packed_data = (const char *)p;
                    break;
                }
            }
        }
        
        if (!packed_data) {
            z_syscall_exit(4);
        }
    }

    // Read metadata (after the marker)
    size_t original_size = *(size_t *)(packed_data + COMP_MARKER_LEN);
    uint64_t entry_offset = *(uint64_t *)(packed_data + COMP_MARKER_LEN + sizeof(size_t));
    int compressed_size = *(int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));
    const char *compressed_data = packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
    int bcj_applied_flag = 1; // legacy default
    #ifdef CODEC_ZSTD
    // For new zstd layout, an extra int (bcj flag) is stored before the compressed stream.
    {
        const unsigned char *m = (const unsigned char *)compressed_data;
        if (!(m[0]==0x28 && m[1]==0xB5 && m[2]==0x2F && m[3]==0xFD)) {
            bcj_applied_flag = *(int *)compressed_data;
            compressed_data += sizeof(int);
        }
    }
    #endif
    
    // Sanity checks to avoid runaway decompression if metadata is corrupt
    // Restrict to ZSTD builds to avoid increasing size of other dynamic stubs
#ifdef CODEC_ZSTD
    if (original_size == 0 || original_size > (256u * 1024u * 1024u)) {
        z_syscall_exit(90);
    }
    if (compressed_size <= 20 || (size_t)compressed_size >= original_size || (size_t)compressed_size > (128u * 1024u * 1024u)) {
        z_syscall_exit(90);
    }
#endif
    #ifdef CODEC_ZSTD
    // Zstd magic check (little-endian 0xFD2FB528)
    {
        const unsigned char *m = (const unsigned char*)compressed_data;
        if (!(m[0]==0x28 && m[1]==0xB5 && m[2]==0x2F && m[3]==0xFD)) {
            z_syscall_exit(4);
        }
    }
    // Zstd header preflight: if content size is known and mismatches, abort early
    {
        long long cs = zstd_peek_content_size((const unsigned char*)compressed_data, (size_t)compressed_size);
        if (cs > 0 && (size_t)cs != original_size) {
            z_syscall_exit(4);
        }
    }
    #endif

    // Allocate memory for the decompressed ELF
    // CRITICAL: LZ4 needs MFLIMIT (12) extra bytes!
    // Add codec-specific slack (ZSTD may require more depending on window/debug)
#ifdef CODEC_ZSTD
    // Safety: add large slack for Zstd block overrun (e.g., ZSTD_BLOCKSIZE_MAX ~128 KiB)
    size_t alloc_size = original_size + (64u << 20) + (128u << 10);
#elif defined(CODEC_SHRINKLER)
    // z_syscall_write(1, "[shr] alloc-pre\n", 16);
    // Shrinkler ASM can transiently advance output beyond target due to long matches.
    // Provide generous slack to avoid protection faults during decode.
    size_t alloc_size = original_size + (32u << 20);  // +32 MiB
#elif defined(CODEC_DENSITY)
    // Density needs extra buffer space for internal processing
    size_t alloc_size = original_size + (original_size >> 2) + 131072;
#else
    size_t alloc_size = original_size + 65536;  // +64KB safety margin for Snappy/LZ4 edge cases
#endif
    #ifdef CODEC_SHRINKLER
    if (alloc_size < original_size + (32u << 20)) {
        alloc_size = original_size + (32u << 20);
    }
    // z_syscall_write(1, "[shr] alloc-map\n", 16);
    #endif
    void *combined_data = (void *)z_syscall_mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)combined_data < 0) {
        z_syscall_exit(3);
    }
    #ifdef CODEC_SHRINKLER
    // z_syscall_write(1, "[shr] alloc-ok\n", 15);
    #endif

////--------DECOMPRESSION ---> utiliser des directives de compil pour lz4, apultra, exomizer etc...
    // Decompression
    
    #ifdef CODEC_SNAPPY
    // Ensure Varint header announces original_size, otherwise abort early (code 4)
    {
        size_t hdr = snappy_get_uncompressed_length((const unsigned char*)compressed_data, (size_t)compressed_size);
        if (hdr != original_size) {
            z_syscall_exit(4);
        }
    }
    #elif defined(CODEC_DOBOZ)
    // Ensure DoboZ header reports original_size, otherwise abort early (code 4)
    {
        size_t hdr = doboz_get_uncompressed_size((const void*)compressed_data, (size_t)compressed_size);
        if (hdr != (size_t)original_size) {
            z_syscall_exit(4);
        }
    }
    #elif defined(CODEC_QLZ)
    // Ensure QuickLZ header (reference) reports original_size
    {
        size_t hdr = qlz_size_decompressed((const char*)compressed_data);
        if (hdr != (size_t)original_size) {
            z_syscall_exit(4);
        }
    }
    #endif
    // Allow decoder to use the full allocated buffer (some codecs need slack)
    /* No external Zstd workspace; keep minimal path */
    #if defined(CODEC_LZMA)
    int decomp_result = lz4_decompress(compressed_data, (char *)combined_data, compressed_size, (int)original_size);
    #elif defined(CODEC_ZSTD)
    int decomp_result = zstd_decompress_c((const unsigned char *)compressed_data, (size_t)compressed_size,
                                          (unsigned char *)combined_data, (size_t)original_size);
    #else
    #ifdef CODEC_SHRINKLER
    // For Shrinkler, provide a full-page zero sentinel after the stream to guard the range decoder
    const char *src_ptr = compressed_data;
    int src_len = compressed_size;
    size_t sentinel = 4096; // one page of zero padding
    void *cbuf_ptr = (void *)z_syscall_mmap(NULL, (size_t)src_len + sentinel, PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)cbuf_ptr < 0) {
        z_syscall_exit(3);
    }
    char *cbuf = (char *)cbuf_ptr;
    for (int i = 0; i < src_len; i++) cbuf[i] = src_ptr[i];
    for (size_t i = 0; i < sentinel; i++) cbuf[src_len + i] = 0;
    int decomp_result = lz4_decompress(cbuf, (char *)combined_data, src_len + (int)sentinel, (int)alloc_size);
    #elif defined(CODEC_BRIEFLZ) || defined(CODEC_ZX7B)
    int decomp_result = lz4_decompress(compressed_data, (char *)combined_data, compressed_size, (int)original_size);
    #else
    int decomp_result = lz4_decompress(compressed_data, (char *)combined_data, compressed_size, (int)alloc_size);
    #endif
    #endif
    // Decompression done
    if (decomp_result < 0) {
         /* For SNAPPY, codec_select returns -snappy_last_error on failure. Propagate it */
         z_syscall_exit(60 - decomp_result); /* 61.. for specific error codes */
    }
    #ifdef CODEC_SHRINKLER
    // z_syscall_write(1, "[shr] done\n", 12);
    #endif
    if (decomp_result != (int)original_size) {
        // Diagnostic exit codes:
        // 5 = decomp_result < original_size (incomplete decompression)
        // 50 = decomp_result > original_size (extra bytes written)
        if (decomp_result < (int)original_size)
            z_syscall_exit(5);
        else
            z_syscall_exit(50);
    }

    // Apply BCJ x86 decode based on stored metadata.
    // For zstd dynamic, use elfz_params_block flag (like other codecs) for consistency.
    // The bcj_applied_flag from packed data is legacy and may not match elfz_flags.
    #ifdef CODEC_ZSTD
    {
        uint64_t pb_addr;
        __asm__ volatile("lea elfz_params_block(%%rip), %0" : "=r"(pb_addr));
        const unsigned char *pb = (const unsigned char *)pb_addr;
        int bcj_flag_from_params = 1; // default legacy behavior
        if (pb[0]=='+' && pb[1]=='z' && pb[2]=='E' && pb[3]=='L' && pb[4]=='F' && pb[5]=='-' && pb[6]=='P' && pb[7]=='R') {
            uint64_t params_version = *(const uint64_t *)(pb + 8);
            bcj_flag_from_params = ((params_version >> 8) & 1ULL) ? 1 : 0; // bit0 of flags
        }
        if (bcj_flag_from_params) {
            bcj_x86_decode((uint8_t*)combined_data, (size_t)original_size, 0);
        }
    }
    #else
    {
        uint64_t pb_addr;
        __asm__ volatile("lea elfz_params_block(%%rip), %0" : "=r"(pb_addr));
        const unsigned char *pb = (const unsigned char *)pb_addr;
        int bcj_applied_flag2 = 1; // default legacy behavior
        if (pb[0]=='+' && pb[1]=='z' && pb[2]=='E' && pb[3]=='L' && pb[4]=='F' && pb[5]=='-' && pb[6]=='P' && pb[7]=='R') {
            uint64_t params_version = *(const uint64_t *)(pb + 8);
            bcj_applied_flag2 = ((params_version >> 8) & 1ULL) ? 1 : 0; // bit0 of flags
        }
        if (bcj_applied_flag2) {
            bcj_x86_decode((uint8_t*)combined_data, (size_t)original_size, 0);
        }
    }
    #endif

    // Ensure the buffer is a valid ELF
    char *elf_data = (char *)combined_data;
    Elf64_Ehdr *ehdr_tmp = (Elf64_Ehdr *)combined_data;  // Temporaire pour validation
    
    if (elf_data[0] == 0x7f && elf_data[1] == 'E' && elf_data[2] == 'L' && elf_data[3] == 'F') {
    } else {
        z_syscall_exit(6);
    }

    // Robust bounds checks on PHDR
    {
        Elf64_Ehdr *elf_hdr_chk = (Elf64_Ehdr *)combined_data;
        uint64_t o_size = (uint64_t)original_size;
        uint64_t phoff = (uint64_t)elf_hdr_chk->e_phoff;
        uint16_t phnum = (uint16_t)elf_hdr_chk->e_phnum;
        uint16_t phentsz = (uint16_t)elf_hdr_chk->e_phentsize;
        if (phentsz != (uint16_t)sizeof(Elf64_Phdr)) {
            z_syscall_exit(91);
        }
        if (phoff > o_size) {
            z_syscall_exit(91);
        }
        uint64_t bytes = (uint64_t)phnum * (uint64_t)phentsz;
        if (phnum == 0 || phentsz == 0 || bytes / (uint64_t)phentsz != (uint64_t)phnum) {
            z_syscall_exit(91);
        }
        uint64_t phend = phoff + bytes;
        if (phend < phoff || phend > o_size) {
            z_syscall_exit(91);
        }
    }

    // CRITICAL DETECTION: ET_EXEC vs ET_DYN
    // ET_EXEC (2) = static binary with absolute addresses
    // ET_DYN  (3) = PIE binary with relative addresses
    int is_pie = (ehdr_tmp->e_type == 3);
    
    // If SEGMENT_COUNT==0 (PIE or static), extract from the decompressed ELF
    if (1 /* SEGMENT_COUNT == 0 */) {
        Elf64_Ehdr *elf_hdr = (Elf64_Ehdr *)combined_data;
        Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)combined_data + elf_hdr->e_phoff);
        // // Compter les segments PT_LOAD (disabled; count not used)
    }
    
    //  Reserve all space first, then map each segment
    // Calculate total required size and maximum alignment
    uint64_t total_size = 0;
    uint64_t max_align = 0x1000;  // at least 4KB
    if (1 /* SEGMENT_COUNT == 0 */) {
        // PIE: compute from the decompressed ELF
        Elf64_Ehdr *elf_hdr = (Elf64_Ehdr *)combined_data;
        Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)combined_data + elf_hdr->e_phoff);
        for (int i = 0; i < elf_hdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_LOAD) {
                uint64_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
                if (seg_end > total_size) total_size = seg_end;
                // Track the alignment required by this segment
                if (phdr[i].p_align > max_align) max_align = phdr[i].p_align;
            }
        }
    }
    
    //  Round total_size up to the next page!
    // Without this, trailing bytes of the last segment will not be mapped
    total_size = (total_size + 0xFFF) & ~0xFFF;
    
    // Map at a HIGH address with alignment ≥ max_align
    // Normalize max_align to a power of two (at least 4KB)
    if (max_align < 0x1000) max_align = 0x1000;
    // Round up to the next power of two if needed
    if ((max_align & (max_align - 1)) != 0) {
        uint64_t p = 0x1000;
        while (p < max_align) p <<= 1;
        max_align = p;
    }
    // Reserve a bit more to allow alignment
    uint64_t reserve_len = total_size + max_align;
    void *reserve = (void *)z_syscall_mmap(NULL, reserve_len, PROT_NONE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)reserve < 0) {
        z_syscall_exit(6);
    }
    uint64_t base_raw = (uint64_t)reserve;
    uint64_t aligned_addr = (base_raw + (max_align - 1)) & ~(max_align - 1);
    coherent_base = (void *)aligned_addr;
    // Release unused head/tail regions to keep RSS low
    uint64_t reserve_end = base_raw + reserve_len;
    size_t head = (size_t)(aligned_addr - base_raw);
    if (head) {
        z_syscall_munmap((void *)base_raw, head);
    }
    size_t tail = (size_t)(reserve_end - (aligned_addr + total_size));
    if (tail) {
        z_syscall_munmap((void *)(aligned_addr + total_size), tail);
    }

    // Map each segment with MAP_FIXED and correct permissions
    Elf64_Ehdr *elf_for_loop = NULL;
    Elf64_Phdr *phdr_for_loop = NULL;
    
    // Dynamic binaries (PIE): always use headers from the decompressed ELF
    // if (SEGMENT_COUNT == 0) {
    elf_for_loop = (Elf64_Ehdr *)combined_data;
    phdr_for_loop = (Elf64_Phdr *)((char *)combined_data + elf_for_loop->e_phoff);
    // }
    
    // Dynamic-only: number of PT_LOAD entries comes from ELF program headers
    int loop_count = elf_for_loop->e_phnum;
    
    // Holder for the hatch created during mapping
    void *hatch = NULL;
    int hatch_creation_count = 0;
    
    // Whole file decompressed, segments at their original p_offset
    for (int i = 0; i < loop_count; i++) {
        // PIE: read from ELF; static would use segments[]
        uint64_t page_base, map_size;
        int prot;
        uint64_t data_offset, filesz, memsz;
        
        if (1 /* SEGMENT_COUNT == 0 */) {
            // Skip non-PT_LOAD
            if (phdr_for_loop[i].p_type != PT_LOAD) continue;
            
            uint64_t seg_align = phdr_for_loop[i].p_align;
            if (seg_align < 0x1000) seg_align = 0x1000;
            // For safety, normalize seg_align to power of two
            if ((seg_align & (seg_align - 1)) != 0) {
                uint64_t p = 0x1000;
                while (p < seg_align) p <<= 1;
                seg_align = p;
            }
            page_base = phdr_for_loop[i].p_vaddr & ~(seg_align - 1);
            map_size = ((phdr_for_loop[i].p_vaddr + phdr_for_loop[i].p_memsz + (seg_align - 1)) & ~(seg_align - 1)) - page_base;
            prot = 0;
            if (phdr_for_loop[i].p_flags & 4) prot |= PROT_READ;
            if (phdr_for_loop[i].p_flags & 2) prot |= PROT_WRITE;
            if (phdr_for_loop[i].p_flags & 1) prot |= PROT_EXEC;
            data_offset = phdr_for_loop[i].p_offset;
            filesz = phdr_for_loop[i].p_filesz;
            memsz = phdr_for_loop[i].p_memsz;
        } else {
            continue; // skip any static-only iteration
        }
        
        // Calcul d'adresse conditionnel
        void *target_addr;
        if (is_pie) {
            // ET_DYN (PIE): addresses are RELATIVE to coherent_base
            target_addr = (char *)coherent_base + page_base;
        } else {
            continue; // skip static-only mapping
        }
        
       
        // Map with temporary PROT_WRITE to copy data
        void *mapped = (void *)z_syscall_mmap(target_addr, map_size,
                                              prot | PROT_WRITE,
                                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if ((long)mapped < 0 || mapped != target_addr) {
            z_syscall_exit(6);
        }
        
        // Copy the data
        // Dynamic-only: compute page offset from program header vaddr
        uint64_t page_offset = (phdr_for_loop[i].p_vaddr - page_base);
        
        void *dest = (char *)mapped + page_offset;
        
        //  ENTIRE file decompressed!
        // Segments are at their ORIGINAL p_offset, not contiguous!
        if (filesz > 0) {
            // PIE: use the segment p_offset within the full decompressed file
            uint64_t file_offset = phdr_for_loop[i].p_offset;
            void *src = (char *)combined_data + file_offset;
            
            simple_memcpy(dest, src, filesz);
        }
        
        // Zero .bss if needed
        if (memsz > filesz) {
            size_t bss_size = memsz - filesz;
            void *bss_start = (char *)dest + filesz;
            simple_memset(bss_start, 0, bss_size);
        }
        
        // Create the hatch BEFORE the final mprotect
        // The hatch must be created while the page is still writable!
        if (hatch == NULL) {
            uint64_t reloc = is_pie ? (uint64_t)coherent_base : 0;
            void *new_hatch = make_hatch_x86_64(&phdr_for_loop[i], reloc, ~0xFFFUL);
            if (new_hatch != NULL) {
                hatch = new_hatch;
                hatch_creation_count++;
            }
        }
        
        // Restaurer permissions finales (enlever PROT_WRITE si pas dans prot original)
        if (!(prot & PROT_WRITE)) {
            z_syscall_mprotect(mapped, map_size, prot);
        }
    }


    // Find the FIRST SEGMENT (contains the ELF header)
    if (0) {
        // elf_header_offset = segments[0].page_base; // disabled (static-only)
    } else {
        // SEGMENT_COUNT == 0: extract ELF header address from decompressed ELF
        if (is_pie) {
            // For ET_DYN (PIE), header lives at base + 0
            elf_header_offset = 0;
        } else {
            z_syscall_exit(1);
        }
    }

   //  Location de l'ELF header selon le type
    unsigned char *elf_header_location;
    if (is_pie) {
        // ET_DYN: ELF header at coherent_base + offset
        elf_header_location = (unsigned char *)coherent_base + elf_header_offset;
    } else {
        z_syscall_exit(1);
    }

    uint64_t base_offset = (uint64_t)coherent_base;

    if (!coherent_base) {
        z_syscall_exit(1);
    }

    // In-memory execution
    // 1. Appliquer les relocations (critique pour PIE)
    // 2. Trouver DT_INIT_ARRAY[0] ou utiliser e_entry
    // 3. Jump directly there
    // Detect PT_INTERP (dynamic binary)
    int has_pt_interp = 0;
    Elf64_Ehdr *elf_check = (Elf64_Ehdr *)elf_header_location;
    Elf64_Phdr *phdr_check = (Elf64_Phdr *)((char *)elf_header_location + elf_check->e_phoff);
    for (int i = 0; i < elf_check->e_phnum; i++) {
        if (phdr_check[i].p_type == PT_INTERP) {
            has_pt_interp = 1;
            break;
        }
    }
    // Pour les binaires dynamiques (ET_DYN avec PT_INTERP), on ne touche PAS aux R_X86_64_RELATIVE !
    if (is_pie && !has_pt_interp) {
        // PIE without PT_INTERP (rare) - relocations disabled to reduce size
        // apply_complete_relocations(coherent_base, (uint64_t)coherent_base);
    } else if (is_pie && has_pt_interp) {
        // Binaire dynamique normal - NE PAS appliquer les relocations
    }

    // Utiliser la bonne adresse pour ehdr selon le type
    Elf64_Ehdr *ehdr;
    Elf64_Phdr *phdr;
    if (is_pie) {
        // ET_DYN: relative to coherent_base
        ehdr = (Elf64_Ehdr *)((char *)coherent_base + elf_header_offset);
        phdr = (Elf64_Phdr *)((char *)coherent_base + elf_header_offset + ehdr->e_phoff);
    } else {
        z_syscall_exit(1);
    }
    // Determine the entry_point of the decompressed binary
    // ET_DYN (PIE) only
    entry_point = ((uint64_t)coherent_base + ehdr->e_entry);
    
    // Dynamic/static detection helper
    int has_dynamic = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            has_dynamic = 1;
            break;
        }
    }
    
    // If PT_DYNAMIC is present, this is a dynamic binary (even without PT_INTERP)
    if (has_dynamic) {
            if (hatch == NULL) {
                z_syscall_exit(88);
            }
            
            // Copier TOUTE la stack et ajuster les pointeurs
            loader_info_t loader_info = handle_dynamic_interpreter_complete(coherent_base, original_sp, hatch);
            
            if (!loader_info.at_null_entry) {
               z_syscall_exit(1);
            }
            
            uint64_t loader_entry = loader_info.loader_entry;
            // Calculer les vraies valeurs ADRU/LENU du stub
            // --- BEGIN SIZE REDUCTION ---
            uint64_t ADRU = 0;
            uint64_t LENU = 0;
            // --- END SIZE REDUCTION ---
            
            extern void elfz_fold_final_sequence(uint64_t, unsigned long *, uint64_t,
                                                   uint64_t, void *, void *, void *, uint64_t) __attribute__((noreturn));
            
           
            // Call with updated hatch and saved_rdx parameters
            elfz_fold_final_sequence(loader_entry, original_sp, ADRU, LENU, 
                               coherent_base, loader_info.interp_base,
                               loader_info.hatch_ptr, saved_rdx_kernel);
    }  // Fin has_dynamic
    
   
    // Reuse argc, argv, envp extracted earlier
    // Trouver auxv
    unsigned long *sp2 = original_sp;
    sp2++;  // Skip argc
    sp2 += argc + 1;  // Skip argv + NULL
    
    int envc = 0;
    while (envp[envc]) envc++;
    sp2 += envc + 1;  // Skip envp + NULL
    
    // sp2 pointe maintenant vers auxv
    unsigned long *auxv = sp2;

    // Utiliser directement la stack originale
    unsigned long *new_stack = original_sp;  // Stack originale
    unsigned long *new_sp = original_sp;     // Start from current position
    
    // Modifier l'AUXV EN PLACE sur la stack originale
    // Pas de copie, juste modification des valeurs AUXV existantes

    // Update AT_PHDR, AT_PHNUM, AT_PHENT, AT_ENTRY directly in auxv
    uint64_t at_phdr = (uint64_t)coherent_base + ((Elf64_Ehdr *)coherent_base)->e_phoff;
    uint64_t at_entry = entry_point;
    int j = 0;
    while (auxv[j * 2] != 0) {  // Walk until AT_NULL
        unsigned long tag = auxv[j * 2];
        if (tag == AT_PHDR) {
            auxv[j * 2 + 1] = at_phdr;
        }
        else if (tag == AT_PHNUM) {
            auxv[j * 2 + 1] = ((Elf64_Ehdr *)coherent_base)->e_phnum;
        }
        else if (tag == AT_PHENT) {
            auxv[j * 2 + 1] = ((Elf64_Ehdr *)coherent_base)->e_phentsize;
        }
        else if (tag == AT_ENTRY) {
            auxv[j * 2 + 1] = at_entry;
        } else if (tag == AT_BASE) {
            auxv[j * 2 + 1] = 0;
        }
        j++;
    }

    unsigned long *argc_ptr = original_sp;  // argc already in place

    // SAUT DIRECT SIMPLE : Restaurer stack et sauter vers binaire
    __asm__ volatile (
        "mov %0, %%rsp\n"              // rsp = stack originale (argc, argv, envp, auxv)
        "xor %%rdx, %%rdx\n"           // Nettoyer rdx
        "xor %%rbx, %%rbx\n"           // Nettoyer rbx  
        "xor %%rcx, %%rcx\n"           // Nettoyer rcx
        "xor %%rbp, %%rbp\n"           // Nettoyer rbp
        "xor %%r8, %%r8\n"             // Nettoyer r8
        "xor %%r9, %%r9\n"             // Nettoyer r9
        "xor %%r10, %%r10\n"           // Nettoyer r10
        "xor %%r11, %%r11\n"           // Nettoyer r11
        "xor %%r12, %%r12\n"           // Nettoyer r12
        "xor %%r13, %%r13\n"           // Nettoyer r13
        "xor %%r15, %%r15\n"           // Nettoyer r15
        "jmp *%1\n"                    // Saut direct vers binaire
        :
        : "r" (argc_ptr), "r" (entry_point)
        : "memory"
    );
    // Ne devrait jamais arriver ici
    z_syscall_exit(1);
}

// Wrapper for start.S (receives pointer to stack structure)
// Structure: [0]=entry_point, [8]=argc_ptr, [16]=original_sp, [24]=stub_addr, [32]=saved_rdx
ELFZ_ALIGN_STACK void elfz_main_wrapper(unsigned long *stack_frame) {
    // Extract original_sp and saved_rdx from the structure
    unsigned long *original_sp = (unsigned long *)stack_frame[2];
    uint64_t saved_rdx = stack_frame[4];
    stub_main_common(original_sp, saved_rdx);
}

// Wrapper for stub_dynamic_full.S (receives original_sp directly)
int stub_dynamic_entry(unsigned long *original_sp) {
    // stub_dynamic_full.S ne sauvegarde pas rdx, utiliser 0
    stub_main_common(original_sp, 0);
    return 0;  // Ne devrait jamais arriver ici
}
