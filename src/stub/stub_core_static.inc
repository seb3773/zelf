#include <stdint.h>
#include <stddef.h>
#include "stub_vars.h"
#include "codec_select.h"
#include "codec_marker.h"
#include "bcj_x86_min.h"

// Noreturn helper for static binaries: restore original stack and jump to e_entry
void static_final_transfer(void *entry, void *orig_sp);

#include "stub_defs.h"
#include "stub_utils.h"
#include "stub_syscalls.h"
#include "stub_elf.h"
#include "stub_reloc.h"

// Minimal params block
#if defined(ELFZ_STUB_PASSWORD)
__attribute__((used, section(".rodata.elfz_params")))
static const unsigned char elfz_params_block[] = {
    '+','z','E','L','F','-','P','R',
    2,0,0,0,0,0,0,0,                  // version = 2
    0,0,0,0,0,0,0,0,                  // virtual_start (to be patched)
    0,0,0,0,0,0,0,0,                  // packed_data_vaddr (to be patched)
    0,0,0,0,0,0,0,0,                  // salt (to be patched)
    0,0,0,0,0,0,0,0                   // pwd_obfhash (to be patched)
};
#else
__attribute__((used, section(".rodata.elfz_params")))
static const unsigned char elfz_params_block[] = {
    '+','z','E','L','F','-','P','R',
    1,0,0,0,0,0,0,0,                  // version = 1
    0,0,0,0,0,0,0,0,                  // virtual_start (to be patched)
    0,0,0,0,0,0,0,0,                  // packed_data_vaddr (to be patched)
    0,0,0,0,0,0,0,0                   // reserved
};
#endif

// Simple formatting helper for debugging
static void debug_print_metadata(size_t original_size, int compressed_size, uint64_t entry_offset) {
    char dec1[16];
    simple_decimal_string(original_size, dec1);
    char dec2[16];
    simple_decimal_string(compressed_size, dec2);
    char hex[32];
    simple_hex_string(entry_offset, hex);
}

// Invoked from ASM to prepare the final handoff sequence (kept for linkage)
uint64_t __attribute__((noinline)) elfz_fold_exact_setup(
    uint64_t entry_point, 
    unsigned long *original_sp, 
    uint64_t ADRU, 
    uint64_t LENU,
    void *elf_base,
    void *interp_base,
    void *hatch_ptr,
    uint64_t saved_rdx) {
    
    void *hatch = hatch_ptr;
    // uint64_t saved_rdx_kernel = saved_rdx; // unused

    unsigned long *auxv_ptr = original_sp;
    auxv_ptr++;  // Skip argc
    while (*auxv_ptr != 0) auxv_ptr++;  // Skip argv
    auxv_ptr++;  // Skip NULL
    while (*auxv_ptr != 0) auxv_ptr++;  // Skip envp
    auxv_ptr++;  // Skip NULL
    
    Elf64_auxv_t *auxv = (Elf64_auxv_t *)auxv_ptr;
    
    Elf64_Ehdr *main_ehdr = (Elf64_Ehdr *)elf_base;
    uint64_t main_phdr = (uint64_t)elf_base + main_ehdr->e_phoff;
    uint64_t main_entry = (uint64_t)elf_base + main_ehdr->e_entry;
    
    auxv_up(auxv, AT_PHDR, main_phdr);
    auxv_up(auxv, AT_PHNUM, main_ehdr->e_phnum);
    auxv_up(auxv, AT_PHENT, main_ehdr->e_phentsize);
    auxv_up(auxv, AT_ENTRY, main_entry);
    auxv_up(auxv, AT_BASE, (uint64_t)interp_base);
    auxv_up(auxv, AT_NULL, (uint64_t)hatch_ptr);
    
    Elf64_auxv_t *at_null = auxv;
    while (at_null->a_type != AT_NULL) at_null++;
    
    uint64_t at_null_entry_addr = (uint64_t)&at_null->a_un.a_val;

    if (entry_point >= ADRU && entry_point < ADRU + LENU) {
        z_syscall_exit(99);
    }
    
    uint64_t hatch_addr = (uint64_t)hatch;
    if (hatch_addr >= ADRU && hatch_addr < ADRU + LENU) {
        z_syscall_exit(98);
    }

    return at_null_entry_addr;
}

extern void _start(void);
extern char _end[];

#if defined(__GNUC__)
#  define ELFZ_ALIGN_STACK __attribute__((force_align_arg_pointer))
#else
#  define ELFZ_ALIGN_STACK
#endif
ELFZ_ALIGN_STACK void elfz_main_wrapper(unsigned long *stack_frame);
ELFZ_ALIGN_STACK int stub_dynamic_entry(unsigned long *original_sp);

static void stub_main_common(unsigned long *original_sp, uint64_t saved_rdx_kernel) {
    const unsigned char *pb;
    {
        uint64_t pb_addr;
        __asm__ volatile("lea elfz_params_block(%%rip), %0" : "=r"(pb_addr));
        pb = (const unsigned char *)pb_addr;
    }

#if defined(ELFZ_STUB_PASSWORD)
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
    if (fd < 0) fd = 0; 
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

    uint64_t entry_point = 0;
    uint64_t binary_entry_point = 0;
    int is_static = 1; 
    void *coherent_base = NULL;
    uint64_t elf_header_offset = 0;
    
    // int argc = *(original_sp); // unused
    // char **argv = (char **)(original_sp + 1); // unused
    // char **envp = argv + argc + 1; // unused

    uint64_t current_start_addr;
    __asm__ volatile("lea _start(%%rip), %0" : "=r" (current_start_addr));

    uint64_t params_virtual_start = 0;
    uint64_t params_packed_data_vaddr = 0;
    uint64_t params_version = 0;
    int bcj_applied_flag = 1; 
    int have_params = 0;

    if (pb[0]=='+' && pb[1]=='z' && pb[2]=='E' && pb[3]=='L' && pb[4]=='F' && pb[5]=='-' && pb[6]=='P' && pb[7]=='R') {
        have_params = 1;
        params_version = *(const uint64_t *)(pb + 8);
        params_virtual_start = *(const uint64_t *)(pb + 16);
        params_packed_data_vaddr = *(const uint64_t *)(pb + 24);
        bcj_applied_flag = ((params_version >> 8) & 1ULL) ? 1 : 0;
    }
    if (!have_params) {
        z_syscall_exit(11);
    }

    uint64_t virtual_start = params_virtual_start;
    uint64_t aslr_offset = current_start_addr - virtual_start;

    if (params_virtual_start == 0 || params_packed_data_vaddr == 0) {
        z_syscall_exit(65);
    }
    const char *packed_data = (const char *)(params_packed_data_vaddr + aslr_offset);

#if defined(CODEC_ZSTD)
    {
        const char *m = COMP_MARKER;
        const unsigned char *pp = (const unsigned char *)packed_data;
        int k = 0;
        while (k < COMP_MARKER_LEN && pp[k] == (unsigned char)m[k]) k++;
        if (k != COMP_MARKER_LEN) {
            z_syscall_exit(66);
        }
    }
#endif

    size_t original_size = *(size_t *)(packed_data + COMP_MARKER_LEN);
    uint64_t entry_offset = *(uint64_t *)(packed_data + COMP_MARKER_LEN + sizeof(size_t));
    int compressed_size = *(int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));
    const char *compressed_data = packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
    
    if (original_size == 0 || original_size > 0x20000000) z_syscall_exit(3);
    if (compressed_size <= 0 || compressed_size > 0x20000000) z_syscall_exit(3);

    debug_print_metadata(original_size, compressed_size, entry_offset);

#if defined(CODEC_ZSTD)
    if ((uint64_t)compressed_data == 0) z_syscall_exit(67);
    uint32_t zstd_magic = *(const uint32_t *)compressed_data;
    if (zstd_magic != 0xFD2FB528u) z_syscall_exit(67);
#endif

    #if defined(CODEC_SHRINKLER)
    size_t alloc_size = original_size + (32u << 20); 
    #elif defined(CODEC_DENSITY)
    // Density needs extra buffer space for internal processing
    size_t alloc_size = original_size + (original_size >> 2) + 131072;
    #else
    size_t alloc_size = original_size + 4096;
    #endif
    void *combined_data = (void *)z_syscall_mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)combined_data < 0) {
        z_syscall_exit(3);
    }

#if defined(CODEC_ZSTD)
    const size_t align = 32;
    const size_t slop  = 64;
    size_t src_buf_sz = (size_t)compressed_size + align + slop;
    void *src_map = (void *)z_syscall_mmap(NULL, src_buf_sz, PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)src_map < 0) z_syscall_exit(3);
    char *src_aligned = (char *)(((uint64_t)(uintptr_t)src_map + (align - 1)) & ~(uint64_t)(align - 1));
    simple_memcpy(src_aligned, compressed_data, (size_t)compressed_size);
    int decomp_result = lz4_decompress(src_aligned, (char *)combined_data, compressed_size, original_size);
    (void)z_syscall_munmap(src_map, src_buf_sz);
#else
    #if defined(CODEC_SHRINKLER)
    const char *src_ptr = compressed_data;
    int src_len = compressed_size;
    size_t sentinel = 4096;
    void *cbuf_ptr = (void *)z_syscall_mmap(NULL, (size_t)src_len + sentinel, PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)cbuf_ptr < 0) z_syscall_exit(3);
    char *cbuf = (char *)cbuf_ptr;
    simple_memcpy(cbuf, src_ptr, src_len);
    for (size_t i = 0; i < sentinel; i++) cbuf[src_len + i] = 0;
    int decomp_result = lz4_decompress(cbuf, (char *)combined_data, src_len + (int)sentinel, (int)alloc_size);
    #elif defined(CODEC_DENSITY)
    int decomp_result = lz4_decompress(compressed_data, (char *)combined_data, compressed_size, (int)alloc_size);
    #else
    int decomp_result = lz4_decompress(compressed_data, (char *)combined_data, compressed_size, original_size);
    #endif
#endif
#if defined(CODEC_DENSITY)
    if (decomp_result <= 0) z_syscall_exit(5);
#else
    if (decomp_result != original_size) z_syscall_exit(5);
#endif

    if (bcj_applied_flag) {
        bcj_x86_decode((uint8_t*)combined_data, (size_t)original_size, 0);
    }

    char *elf_data = (char *)combined_data;
    Elf64_Ehdr *ehdr_tmp = (Elf64_Ehdr *)combined_data;
    
    if (elf_data[0] != 0x7f || elf_data[1] != 'E' || elf_data[2] != 'L' || elf_data[3] != 'F') {
        z_syscall_exit(6);
    }

    int is_pie = (ehdr_tmp->e_type == 3);

    uint64_t total_size = 0;
    if (SEGMENT_COUNT == 0) {
        Elf64_Ehdr *elf_hdr = (Elf64_Ehdr *)combined_data;
        Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)combined_data + elf_hdr->e_phoff);
        for (int i = 0; i < elf_hdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_LOAD) {
                uint64_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
                if (seg_end > total_size) total_size = seg_end;
            }
        }
    } else {
        for (int i = 0; i < SEGMENT_COUNT; i++) {
            uint64_t seg_end = segments[i].page_base + segments[i].map_size;
            if (seg_end > total_size) total_size = seg_end;
        }
    }
    
    total_size = (total_size + 0xFFF) & ~0xFFF;
    
    void *target_base_addr = NULL;
    coherent_base = (void *)z_syscall_mmap(target_base_addr, total_size,
                                           PROT_NONE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)coherent_base < 0) z_syscall_exit(6);

    Elf64_Ehdr *elf_for_loop = NULL;
    Elf64_Phdr *phdr_for_loop = NULL;
    
    if (SEGMENT_COUNT == 0) {
        elf_for_loop = (Elf64_Ehdr *)combined_data;
        phdr_for_loop = (Elf64_Phdr *)((char *)combined_data + elf_for_loop->e_phoff);
    }
    
    int loop_count = (SEGMENT_COUNT == 0) ? elf_for_loop->e_phnum : SEGMENT_COUNT;
    
    for (int i = 0; i < loop_count; i++) {
        uint64_t page_base, map_size;
        int prot;
        uint64_t data_offset, filesz, memsz;
        
        if (SEGMENT_COUNT == 0) {
            if (phdr_for_loop[i].p_type != PT_LOAD) continue;
            uint64_t seg_align = phdr_for_loop[i].p_align;
            if (seg_align < 0x1000) seg_align = 0x1000;
            if ((seg_align & (seg_align - 1)) != 0) {
                uint64_t p2 = 0x1000; while (p2 < seg_align) p2 <<= 1; seg_align = p2;
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
            page_base = segments[i].page_base;
            map_size = segments[i].map_size;
            prot = segments[i].prot;
            data_offset = segments[i].data_offset;
            filesz = segments[i].filesz;
            memsz = segments[i].memsz;
        }
        
        void *target_addr;
        if (is_pie) {
            target_addr = (char *)coherent_base + page_base;
        } else {
            target_addr = (void *)page_base;
        }
        
        void *mapped = (void *)z_syscall_mmap(target_addr, map_size,
                                              prot | PROT_WRITE,
                                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if ((long)mapped < 0 || mapped != target_addr) z_syscall_exit(6);
        
        uint64_t page_offset = (SEGMENT_COUNT == 0) ? 
            (phdr_for_loop[i].p_vaddr - page_base) : segments[i].page_offset;
        
        void *dest = (char *)mapped + page_offset;
        
        if (filesz > 0 && SEGMENT_COUNT == 0) {
            uint64_t file_offset = phdr_for_loop[i].p_offset;
            void *src = (char *)combined_data + file_offset;
            simple_memcpy(dest, src, filesz);
        } else if (filesz > 0) {
            void *src = (char *)combined_data + data_offset;
            simple_memcpy(dest, src, filesz);
        }
        
        if (memsz > filesz) {
            size_t bss_size = memsz - filesz;
            void *bss_start = (char *)dest + filesz;
            simple_memset(bss_start, 0, bss_size);
        }
        
        if (!(prot & PROT_WRITE)) {
            z_syscall_mprotect(mapped, map_size, prot);
        }
    }

    if (SEGMENT_COUNT > 0) {
        elf_header_offset = segments[0].page_base;
    } else {
        if (is_pie) {
            elf_header_offset = 0;
        } else {
            Elf64_Ehdr *eh_exec = (Elf64_Ehdr *)combined_data;
            Elf64_Phdr *ph_exec = (Elf64_Phdr *)((char *)combined_data + eh_exec->e_phoff);
            unsigned char *hdr = NULL;
            for (int i = 0; i < eh_exec->e_phnum; i++) {
                Elf64_Phdr *p = &ph_exec[i];
                if (p->p_type != PT_LOAD) continue;
                if (p->p_offset <= 0 && (uint64_t)0 < p->p_offset + p->p_filesz) {
                    hdr = (unsigned char *)(p->p_vaddr - p->p_offset);
                    break;
                }
            }
            if (!hdr) {
                uint64_t min_vaddr = (uint64_t)-1;
                for (int i = 0; i < eh_exec->e_phnum; i++) {
                    if (ph_exec[i].p_type == PT_LOAD && ph_exec[i].p_vaddr < min_vaddr)
                        min_vaddr = ph_exec[i].p_vaddr;
                }
                hdr = (unsigned char *)min_vaddr;
            }
            elf_header_offset = (uint64_t)hdr;
        }
    }

    unsigned char *elf_header_location;
    if (is_pie) {
        elf_header_location = (unsigned char *)coherent_base + elf_header_offset;
    } else {
        elf_header_location = (unsigned char *)elf_header_offset;
    }

#if !defined(CODEC_ZSTD)
    int hdr_prot = PROT_READ;
    {
        uint64_t hdr_page = (uint64_t)elf_header_location & ~0xFFFULL;
        if (SEGMENT_COUNT > 0) {
            for (int i = 0; i < SEGMENT_COUNT; i++) {
                uint64_t seg_start = is_pie ? (uint64_t)coherent_base + (uint64_t)segments[i].page_base
                                            : (uint64_t)segments[i].page_base;
                uint64_t seg_end = seg_start + (uint64_t)segments[i].map_size;
                if (hdr_page >= seg_start && hdr_page < seg_end) {
                    hdr_prot = (int)segments[i].prot;
                    if (!(hdr_prot & PROT_READ)) hdr_prot |= PROT_READ;
                    break;
                }
            }
        } else {
            Elf64_Ehdr *eh_exec = (Elf64_Ehdr *)combined_data;
            Elf64_Phdr *ph_exec = (Elf64_Phdr *)((char *)combined_data + eh_exec->e_phoff);
            for (int i = 0; i < eh_exec->e_phnum; i++) {
                Elf64_Phdr *p = &ph_exec[i];
                if (p->p_type != PT_LOAD) continue;

                uint64_t seg_align = p->p_align;
                if (seg_align < 0x1000) seg_align = 0x1000;
                if ((seg_align & (seg_align - 1)) != 0) {
                    uint64_t ap = 0x1000; while (ap < seg_align) ap <<= 1; seg_align = ap;
                }

                uint64_t page_base = p->p_vaddr & ~(seg_align - 1);
                uint64_t map_size = ((p->p_vaddr + p->p_memsz + (seg_align - 1)) & ~(seg_align - 1)) - page_base;
                uint64_t seg_start = is_pie ? (uint64_t)coherent_base + page_base : page_base;
                uint64_t seg_end = seg_start + map_size;
                if (hdr_page >= seg_start && hdr_page < seg_end) {
                    int pprot = 0;
                    if (p->p_flags & 4) pprot |= PROT_READ;
                    if (p->p_flags & 2) pprot |= PROT_WRITE;
                    if (p->p_flags & 1) pprot |= PROT_EXEC;
                    if (!(pprot & PROT_READ)) pprot |= PROT_READ;
                    hdr_prot = pprot;
                    break;
                }
            }
        }
    }
    if (z_syscall_mprotect((void*)((uint64_t)elf_header_location & ~0xFFF), 4096, hdr_prot) != 0) {
        z_syscall_exit(1);
    }
#endif

    if (elf_header_location[0] != 0x7f || elf_header_location[1] != 'E' || 
        elf_header_location[2] != 'L' || elf_header_location[3] != 'F') {
        z_syscall_exit(1);
    }

    uint64_t base_offset = (uint64_t)coherent_base;
    if (!coherent_base) z_syscall_exit(1);

    Elf64_Ehdr *ehdr;
    if (is_pie) {
        ehdr = (Elf64_Ehdr *)((char *)coherent_base + elf_header_offset);
    } else {
        ehdr = (Elf64_Ehdr *)elf_header_offset;
    }
    
    if (is_static) {
        if (is_pie) {
            binary_entry_point = (uint64_t)coherent_base + ehdr->e_entry;
            entry_point = binary_entry_point;
        } else {
            binary_entry_point = ehdr->e_entry;
            entry_point = binary_entry_point;
        }
        
        Elf64_auxv_t *auxv = get_auxv_from_original_stack(original_sp);
        auxv_up(auxv, AT_ENTRY, entry_point);
        
        uint64_t at_phdr;
        if (is_pie) {
            at_phdr = (uint64_t)coherent_base + ehdr->e_phoff;
        } else {
            at_phdr = elf_header_offset + ehdr->e_phoff;
        }
        auxv_up(auxv, AT_PHDR, at_phdr);
        auxv_up(auxv, AT_PHNUM, ehdr->e_phnum);
        auxv_up(auxv, AT_PHENT, ehdr->e_phentsize);
        auxv_up(auxv, AT_BASE, 0);

        static_final_transfer((void *)entry_point, (void *)original_sp);
        z_syscall_exit(1);
    }
}

ELFZ_ALIGN_STACK void elfz_main_wrapper(unsigned long *stack_frame) {
    unsigned long *original_sp = (unsigned long *)stack_frame[2];
    uint64_t saved_rdx = stack_frame[4];
    stub_main_common(original_sp, saved_rdx);
}

int stub_dynamic_entry(unsigned long *original_sp) {
    stub_main_common(original_sp, 0);
    return 0;
}
