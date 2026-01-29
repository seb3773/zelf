#ifndef CONSOLE_PROGRESS_H
#define CONSOLE_PROGRESS_H
#ifdef CONSOLE_USE_RAW_CLONE
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
#endif

// unified console spinners and progress bars (header-only)
// usage:
//   In exactly one C file:
//     #define CONSOLE_PROGRESS_IMPLEMENTATION
//     #include "console_progress.h"
//   and link with -pthread.
//
// compile-time selection (OPT-IN):
//   >> by default, ALL variants are compiled (if you don't specify anything).
//   
// ** to compile ONLY specific variants, define them before including:
//  #define CONSOLE_USE_SPINNER_2
//  #define CONSOLE_USE_PROGRESS_4
//  #include "console_progress.h"
//   
//   or via command line:  gcc -DCONSOLE_USE_SPINNER_2 -DCONSOLE_USE_PROGRESS_4 ...
//
//   ** only the specified variants will be included in the binary.
//
// API:
//   spinners (id in [1..12]):
//     void start_spinner(const char *message, int spinner_id);
//     void stop_spinner(int spinner_id);
//
//   progress bars (id in [1..12]):
//     void start_progress(const char *message, int progress_id);
//     void update_progress(int percentage, int progress_id);
//     void stop_progress(int progress_id);

// minimal dependencies
#include <time.h>      // nanosleep (required)
#ifndef CONSOLE_USE_RAW_CLONE
    #include <pthread.h>   // pthread_create, pthread_join (required if not using clone)
#else
    // raw clone() syscall - no pthread dependency
    #include <sched.h>     // CLONE_* flags
    #include <sys/mman.h>  // mmap for thread stack
    #include <unistd.h>    // syscall
    #include <sys/syscall.h> // SYS_clone, SYS_futex
    #include <linux/futex.h> // FUTEX_WAIT, FUTEX_WAKE
#endif
// Optional: define CONSOLE_NO_STDIO to use raw syscalls (advanced, Linux only)
#ifndef CONSOLE_NO_STDIO
    #include <stdio.h>  // printf, fflush
#else
    #include <unistd.h> // write syscall
#endif
#ifndef CONSOLE_SPINNER_SPEED_NS
#define CONSOLE_SPINNER_SPEED_NS 100000000L  // 100ms
#endif
#ifndef CONSOLE_PROGRESS_REFRESH_NS
#define CONSOLE_PROGRESS_REFRESH_NS 50000000L // 50ms
#endif
#ifndef CONSOLE_PROGRESS_BAR_WIDTH
#define CONSOLE_PROGRESS_BAR_WIDTH 25
#endif

// Runtime gating (ELFZ packer): when set, suppress all progress/spinner output
// Declared and defined in packer: extern int g_no_progress;
extern int g_no_progress;

// OPT-IN LOGIC: if any CONSOLE_USE_* is defined, only those are enabled.
// otherwise, all variants are enabled by default.
#if defined(CONSOLE_USE_SPINNER_1) || defined(CONSOLE_USE_SPINNER_2) || \
    defined(CONSOLE_USE_SPINNER_3) || defined(CONSOLE_USE_SPINNER_4) || \
    defined(CONSOLE_USE_SPINNER_5) || defined(CONSOLE_USE_SPINNER_6) || \
    defined(CONSOLE_USE_SPINNER_7) || defined(CONSOLE_USE_SPINNER_8) || \
    defined(CONSOLE_USE_SPINNER_9) || defined(CONSOLE_USE_SPINNER_10) || \
    defined(CONSOLE_USE_SPINNER_11) || defined(CONSOLE_USE_SPINNER_12) || \
    defined(CONSOLE_USE_PROGRESS_1) || defined(CONSOLE_USE_PROGRESS_2) || \
    defined(CONSOLE_USE_PROGRESS_3) || defined(CONSOLE_USE_PROGRESS_4) || \
    defined(CONSOLE_USE_PROGRESS_5) || defined(CONSOLE_USE_PROGRESS_6) || \
    defined(CONSOLE_USE_PROGRESS_7) || defined(CONSOLE_USE_PROGRESS_8) || \
    defined(CONSOLE_USE_PROGRESS_9) || defined(CONSOLE_USE_PROGRESS_10) || \
    defined(CONSOLE_USE_PROGRESS_11) || defined(CONSOLE_USE_PROGRESS_12)
    #define CONSOLE_EXPLICIT_SELECTION
#endif
#ifdef CONSOLE_EXPLICIT_SELECTION
     #ifdef CONSOLE_USE_SPINNER_1
        #define CONSOLE_ENABLE_SPINNER_1 1
    #else
        #define CONSOLE_ENABLE_SPINNER_1 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_2
        #define CONSOLE_ENABLE_SPINNER_2 1
    #else
        #define CONSOLE_ENABLE_SPINNER_2 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_3
        #define CONSOLE_ENABLE_SPINNER_3 1
    #else
        #define CONSOLE_ENABLE_SPINNER_3 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_4
        #define CONSOLE_ENABLE_SPINNER_4 1
    #else
        #define CONSOLE_ENABLE_SPINNER_4 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_5
        #define CONSOLE_ENABLE_SPINNER_5 1
    #else
        #define CONSOLE_ENABLE_SPINNER_5 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_6
        #define CONSOLE_ENABLE_SPINNER_6 1
    #else
        #define CONSOLE_ENABLE_SPINNER_6 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_7
        #define CONSOLE_ENABLE_SPINNER_7 1
    #else
        #define CONSOLE_ENABLE_SPINNER_7 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_8
        #define CONSOLE_ENABLE_SPINNER_8 1
    #else
        #define CONSOLE_ENABLE_SPINNER_8 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_9
        #define CONSOLE_ENABLE_SPINNER_9 1
    #else
        #define CONSOLE_ENABLE_SPINNER_9 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_10
        #define CONSOLE_ENABLE_SPINNER_10 1
    #else
        #define CONSOLE_ENABLE_SPINNER_10 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_11
        #define CONSOLE_ENABLE_SPINNER_11 1
    #else
        #define CONSOLE_ENABLE_SPINNER_11 0
    #endif
    #ifdef CONSOLE_USE_SPINNER_12
        #define CONSOLE_ENABLE_SPINNER_12 1
    #else
        #define CONSOLE_ENABLE_SPINNER_12 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_1
        #define CONSOLE_ENABLE_PROGRESS_1 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_1 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_2
        #define CONSOLE_ENABLE_PROGRESS_2 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_2 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_3
        #define CONSOLE_ENABLE_PROGRESS_3 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_3 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_4
        #define CONSOLE_ENABLE_PROGRESS_4 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_4 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_5
        #define CONSOLE_ENABLE_PROGRESS_5 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_5 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_6
        #define CONSOLE_ENABLE_PROGRESS_6 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_6 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_7
        #define CONSOLE_ENABLE_PROGRESS_7 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_7 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_8
        #define CONSOLE_ENABLE_PROGRESS_8 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_8 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_9
        #define CONSOLE_ENABLE_PROGRESS_9 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_9 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_10
        #define CONSOLE_ENABLE_PROGRESS_10 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_10 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_11
        #define CONSOLE_ENABLE_PROGRESS_11 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_11 0
    #endif
    #ifdef CONSOLE_USE_PROGRESS_12
        #define CONSOLE_ENABLE_PROGRESS_12 1
    #else
        #define CONSOLE_ENABLE_PROGRESS_12 0
    #endif
#else
    #define CONSOLE_ENABLE_SPINNER_1 1
    #define CONSOLE_ENABLE_SPINNER_2 1
    #define CONSOLE_ENABLE_SPINNER_3 1
    #define CONSOLE_ENABLE_SPINNER_4 1
    #define CONSOLE_ENABLE_SPINNER_5 1
    #define CONSOLE_ENABLE_SPINNER_6 1
    #define CONSOLE_ENABLE_SPINNER_7 1
    #define CONSOLE_ENABLE_SPINNER_8 1
    #define CONSOLE_ENABLE_SPINNER_9 1
    #define CONSOLE_ENABLE_SPINNER_10 1
    #define CONSOLE_ENABLE_SPINNER_11 1
    #define CONSOLE_ENABLE_SPINNER_12 1
    #define CONSOLE_ENABLE_PROGRESS_1 1
    #define CONSOLE_ENABLE_PROGRESS_2 1
    #define CONSOLE_ENABLE_PROGRESS_3 1
    #define CONSOLE_ENABLE_PROGRESS_4 1
    #define CONSOLE_ENABLE_PROGRESS_5 1
    #define CONSOLE_ENABLE_PROGRESS_6 1
    #define CONSOLE_ENABLE_PROGRESS_7 1
    #define CONSOLE_ENABLE_PROGRESS_8 1
    #define CONSOLE_ENABLE_PROGRESS_9 1
    #define CONSOLE_ENABLE_PROGRESS_10 1
    #define CONSOLE_ENABLE_PROGRESS_11 1
    #define CONSOLE_ENABLE_PROGRESS_12 1
#endif
void start_spinner(const char *message, int spinner_id);
void stop_spinner(int spinner_id);
void start_progress(const char *message, int progress_id);
void update_progress(int percentage, int progress_id);
void stop_progress(int progress_id);
#ifdef CONSOLE_PROGRESS_IMPLEMENTATION
#if defined(__x86_64__) && defined(CONSOLE_USE_ASM)
static inline unsigned long console_strlen(const char *s) {
    unsigned long len;
    __asm__ volatile (
        "xor %%rcx, %%rcx\n\t"    // rcx = 0
        "not %%rcx\n\t"              // rcx = -1 
        "xor %%al, %%al\n\t"        // al = 0
        "cld\n\t"                   // clear dir flag
        "repne scasb\n\t"           // scan until null, rcx decrements
        "not %%rcx\n\t"              // rcx = length + 1
        "dec %%rcx"                // rcx = length
        : "=c" (len)                 // out: rcx -> len
        : "D" (s)                // in: rdi = s
        : "al", "cc", "memory"      // clobbers
    );
    return len;
}
#else
static inline unsigned long console_strlen(const char *s) {
    const char *p = s;
    while (*p) ++p;
    return (unsigned long)(p - s);
}
#endif
static inline void console_strncpy(char *dst, const char *src, unsigned long n) {
    unsigned long i;
    for (i = 0; i < n - 1 && src[i]; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}
#ifndef CONSOLE_NO_STDIO
    #define console_write_str(s) printf("%s", s)
    #define console_write_char(c) printf("%c", c)
    #define console_write_int(d) printf("%d", d)
    #define console_flush() fflush(stdout)
    #define console_print_frame(frame, msg) printf("\r%s %s ", frame, msg)
    #define console_print_char_frame(c, msg) printf("\r%c %s ", c, msg)
    #define console_print_clear(width) printf("\r%*s\r", (int)(width), "")
    #define console_print_bar_start() printf("\r[")
    #define console_print_bar_end(pct, msg) printf("] %d%% %s", pct, msg)
    #define console_print_newline() printf("\n")
#else
    static inline void console_write_str(const char *s) {
        unsigned long len = console_strlen(s);
        write(1, s, len);
    }
    static inline void console_write_char(char c) {
        write(1, &c, 1);
    }
    static char g_temp_buffer[32] __attribute__((aligned(8)));
    static inline void console_write_int(int n) {
        int i = 0;
        int neg = 0;
        if (n < 0) { neg = 1; n = -n; }
        if (n == 0) { g_temp_buffer[i++] = '0'; }
        else {
            while (n > 0) {
                g_temp_buffer[i++] = '0' + (n % 10);
                n /= 10;
            }
        }
        if (neg) g_temp_buffer[i++] = '-';
        for (int j = 0; j < i/2; ++j) {
            char tmp = g_temp_buffer[j];
            g_temp_buffer[j] = g_temp_buffer[i-1-j];
            g_temp_buffer[i-1-j] = tmp;
        }
        write(1, g_temp_buffer, i);
    }
    #define console_flush() (void)0
    static inline void console_print_frame(const char *frame, const char *msg) {
        write(1, "\r", 1);
        console_write_str(frame);
        write(1, " ", 1);
        console_write_str(msg);
        write(1, " ", 1);
    }
    static inline void console_print_char_frame(char c, const char *msg) {
        write(1, "\r", 1);
        write(1, &c, 1);
        write(1, " ", 1);
        console_write_str(msg);
        write(1, " ", 1);
    }
    static inline void console_print_clear(unsigned long width) {
        write(1, "\r", 1);
        for (unsigned long i = 0; i < width; ++i) write(1, " ", 1);
        write(1, "\r", 1);
    }
    #define console_print_bar_start() write(1, "\r[", 2)
    static inline void console_print_bar_end(int pct, const char *msg) {
        write(1, "] ", 2);
        console_write_int(pct);
        write(1, "% ", 2);
        console_write_str(msg);
    }
    #define console_print_newline() write(1, "\n", 1)
#endif
#define CONSOLE_FLAG_SPINNER_RUNNING 0x01
#define CONSOLE_FLAG_PROGRESS_RUNNING 0x02
static volatile int g_flags = 0;
#ifndef CONSOLE_USE_RAW_CLONE
    static pthread_t g_spinner_thread;
    static pthread_t g_progress_thread;
#else
    static int g_spinner_tid = 0;
    static int g_progress_tid = 0;
    #define CONSOLE_THREAD_STACK_SIZE (64 * 1024)
    static char g_spinner_stack[CONSOLE_THREAD_STACK_SIZE] __attribute__((aligned(16)));
    static char g_progress_stack[CONSOLE_THREAD_STACK_SIZE] __attribute__((aligned(16)));
#endif
static int g_spinner_id = 0;
static int g_progress_id = 0;
static char g_spinner_message[256] = {0};
static char g_progress_message[256] = {0};
static volatile int g_progress_percentage = 0;
#ifdef CONSOLE_USE_RAW_CLONE
static inline int console_thread_create(int *tid, void *stack, int (*fn)(void *), void *arg) {
    (void)arg;
    unsigned long flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | 
                          CLONE_THREAD | CLONE_SYSVSEM;
    void *stack_top = (char *)stack + CONSOLE_THREAD_STACK_SIZE;
    *tid = clone(fn, stack_top, (int)flags, arg, NULL, NULL, NULL);
    return (*tid > 0) ? 0 : -1;
}
static inline void console_thread_join(int *tid) {
    volatile int *tid_ptr = (volatile int *)tid;
    while (*tid_ptr != 0) {
        syscall(SYS_futex, tid_ptr, FUTEX_WAIT, *tid_ptr, NULL, NULL, 0);
    }
}
static inline void console_thread_exit(int *tid) {
    *tid = 0;
    syscall(SYS_futex, tid, FUTEX_WAKE, 1, NULL, NULL, 0);
    syscall(SYS_exit, 0);
}
#else
typedef struct {
    void *(*start_routine)(void *);
    void *arg;
} console_pthread_wrapper_t;
static inline int console_thread_create_pthread(pthread_t *thread, void *(*fn)(void *), void *arg) {
    return pthread_create(thread, NULL, fn, arg);
}
static inline void console_thread_join_pthread(pthread_t *thread) {
    pthread_join(*thread, NULL);
}
#endif
static void console_hide_cursor(void) {
    console_write_str("\e[?25l");
}
static void console_show_cursor(void) {
    console_write_str("\e[?25h");
}
static void spinner_print_frame(int id, int index) {
    switch (id) {
#if CONSOLE_ENABLE_SPINNER_1
        case 1: {
            const char frames[] = "|/-\\"; // 4 frames
            char c = frames[index % (int)(sizeof(frames) - 1)];
            console_print_char_frame(c, g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_2
        case 2: {
            static const char *frames[] = {"•","✶","✵","✻","✽","✻","✵","✶"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_3
        case 3: {
            static const char *frames[] = {"⠏","⠋","⠉","⠙","⠸","⠼","⠴","⠤","⠦","⠧"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_4
        case 4: {
            static const char *frames[] = {"(●     )","( ●    )","(  ●   )","(   ●  )","(    ● )","(     ●)","(    ● )","(   ●  )","(  ●   )","( ●    )"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_5
        case 5: {
            static const char *frames[] = {"[✸]","[✹]","[✺]","[✻]","[✼]","[✽]","[✾]","[✿]","[✾]","[✽]","[✼]","[✻]","[✺]","[✹]"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_6
        case 6: {
            static const char *frames[] = {"✦    "," ✦   ","  ✦  ","   ✦ ","    ✦","   ✧ ","  ✧  "," ✧   ","✧    "};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_7
        case 7: {
            static const char *frames[] = {"┏(-_-)┛","┗(-_-)┓","┏(-_-)┓","┗(-_-)┛"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_8
        case 8: {
            static const char *frames[] = {"⠋","⠙","⠚","⠞","⠖","⠦","⠴","⠲","⠳","⠓"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_9
        case 9: {
            static const char *frames[] = {"■","□","▪","▫"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_10
        case 10: {
            static const char *frames[] = {"←","↑","→","↓"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_11
        case 11: {
            static const char *frames[] = {
                "▐⠂       ▌","▐⠈       ▌","▐ ⠂      ▌","▐ ⠠      ▌","▐  ⡀     ▌","▐  ⠠     ▌","▐   ⠂    ▌","▐   ⠈    ▌",
                "▐    ⠂   ▌","▐    ⠠   ▌","▐     ⡀  ▌","▐     ⠠  ▌","▐      ⠂ ▌","▐      ⠈ ▌","▐       ⠂▌","▐       ⠠▌","▐       ⡀▌","▐      ⠠ ▌",
                "▐      ⠂ ▌","▐     ⠈  ▌","▐     ⠂  ▌","▐    ⠠   ▌","▐    ⡀   ▌","▐   ⠠    ▌","▐   ⠂    ▌","▐  ⠈     ▌","▐  ⠂     ▌","▐ ⠠      ▌","▐ ⡀      ▌","▐⠠       ▌"
            };
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
#if CONSOLE_ENABLE_SPINNER_12
        case 12: {
            static const char *frames[] = {"▸    "," ▸   ","  ▸  ","   ▸ ","    ▸"};
            int n = (int)(sizeof(frames)/sizeof(frames[0]));
            console_print_frame(frames[index % n], g_spinner_message);
            break;
        }
#endif
        default: {
            console_write_str("\r");
            console_write_str(g_spinner_message);
            break;
        }
    }
    console_flush();
}
#ifdef CONSOLE_USE_RAW_CLONE
static int spinner_thread_func(void *arg) {
#else
static void *spinner_thread_func(void *arg) {
#endif
    (void)arg;
    struct timespec sleep_duration = {0, CONSOLE_SPINNER_SPEED_NS};
    int i = 0;
    console_hide_cursor();
    while (g_flags & CONSOLE_FLAG_SPINNER_RUNNING) {
        spinner_print_frame(g_spinner_id, i);
        i++;
        nanosleep(&sleep_duration, NULL);
    }
    unsigned long msg_len = console_strlen(g_spinner_message);
    console_print_clear(msg_len + 10);
    console_show_cursor();
    console_flush();
#ifdef CONSOLE_USE_RAW_CLONE
    console_thread_exit(&g_spinner_tid);
    return 0;
#else
    return NULL;
#endif
}
void start_spinner(const char *message, int spinner_id) {
    if (g_no_progress) return;
    if (g_flags & CONSOLE_FLAG_SPINNER_RUNNING) {
        g_flags &= ~CONSOLE_FLAG_SPINNER_RUNNING;
#ifdef CONSOLE_USE_RAW_CLONE
        console_thread_join(&g_spinner_tid);
#else
        pthread_join(g_spinner_thread, NULL);
#endif
    }
    if (message) {
        console_strncpy(g_spinner_message, message, sizeof(g_spinner_message));
    } else {
        g_spinner_message[0] = '\0';
    }
    g_spinner_id = spinner_id;
    g_flags |= CONSOLE_FLAG_SPINNER_RUNNING;

#ifdef CONSOLE_USE_RAW_CLONE
    console_thread_create(&g_spinner_tid, g_spinner_stack, spinner_thread_func, NULL);
#else
    pthread_create(&g_spinner_thread, NULL, spinner_thread_func, NULL);
#endif
}
void stop_spinner(int spinner_id) {
    (void)spinner_id;
    if (!(g_flags & CONSOLE_FLAG_SPINNER_RUNNING)) return;
    g_flags &= ~CONSOLE_FLAG_SPINNER_RUNNING;
#ifdef CONSOLE_USE_RAW_CLONE
    console_thread_join(&g_spinner_tid);
#else
    pthread_join(g_spinner_thread, NULL);
#endif
}
static void draw_progress_bar_common(int percent, int id) {
    const int bar_width = CONSOLE_PROGRESS_BAR_WIDTH;
    int filled = (bar_width * percent) / 100;
    console_print_bar_start();
    switch (id) {
#if CONSOLE_ENABLE_PROGRESS_1
        case 1: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("#"); else console_write_str("-");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_2
        case 2: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("█"); else console_write_str(" ");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_3
        case 3: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("•"); else console_write_str(" ");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_4
        case 4: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("▓"); else console_write_str("-");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_5
        case 5: {
            int head_pos = (bar_width * percent) / 100;
            if (percent > 0 && head_pos == 0) head_pos = 1;
            if (percent == 100) head_pos = bar_width;
            for (int i = 0; i < bar_width; ++i) {
                if (i < head_pos) console_write_str("~");
                else if (i == head_pos && percent < 100) console_write_str(">");
                else console_write_str(" ");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_6
        case 6: {
            static const char* grad[] = {"░","▒","▓","█"};
            const int n = (int)(sizeof(grad)/sizeof(grad[0]));
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) {
                    int idx = (i * n) / bar_width;
                    if (idx >= n) idx = n - 1;
                    console_write_str(grad[idx]);
                } else {
                    console_write_str(" ");
                }
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_7
        case 7: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("="); else console_write_str("-");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_8
        case 8: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("▬"); else console_write_str("-");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_9
        case 9: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("➡"); else console_write_str(" ");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_10
        case 10: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("▸"); else console_write_str(" ");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_11
        case 11: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("▌"); else console_write_str(" ");
            }
            break;
        }
#endif
#if CONSOLE_ENABLE_PROGRESS_12
        case 12: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("»"); else console_write_str(" ");
            }
            break;
        }
#endif
        default: {
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) console_write_str("#"); else console_write_str("-");
            }
        }
    }
    console_print_bar_end(percent, g_progress_message);
    console_flush();
}
#ifdef CONSOLE_USE_RAW_CLONE
static int progress_thread_func(void *arg) {
#else
static void *progress_thread_func(void *arg) {
#endif
    (void)arg;
    struct timespec sleep_duration = {0, CONSOLE_PROGRESS_REFRESH_NS};
    int last_drawn = -1;
    console_hide_cursor();
    while (g_flags & CONSOLE_FLAG_PROGRESS_RUNNING) {
        int cur = g_progress_percentage;
        if (cur != last_drawn) {
            last_drawn = cur;
            draw_progress_bar_common(cur, g_progress_id);
        }
        nanosleep(&sleep_duration, NULL);
    }
    /* Draw final state and newline so callers can clear the line predictably */
    draw_progress_bar_common(g_progress_percentage, g_progress_id);
    console_print_newline();
    console_show_cursor();
    console_flush();
#ifdef CONSOLE_USE_RAW_CLONE
    console_thread_exit(&g_progress_tid);
    return 0;
#else
    return NULL;
#endif
}

void start_progress(const char *message, int progress_id) {
    if (g_no_progress) return;
    if (g_flags & CONSOLE_FLAG_PROGRESS_RUNNING) {
        g_flags &= ~CONSOLE_FLAG_PROGRESS_RUNNING;
#ifdef CONSOLE_USE_RAW_CLONE
        console_thread_join(&g_progress_tid);
#else
        pthread_join(g_progress_thread, NULL);
#endif
    }

    if (message) {
        console_strncpy(g_progress_message, message, sizeof(g_progress_message));
    } else {
        g_progress_message[0] = '\0';
    }

    g_progress_id = progress_id;
    g_progress_percentage = 0;
    g_flags |= CONSOLE_FLAG_PROGRESS_RUNNING;
    
#ifdef CONSOLE_USE_RAW_CLONE
    console_thread_create(&g_progress_tid, g_progress_stack, progress_thread_func, NULL);
#else
    pthread_create(&g_progress_thread, NULL, progress_thread_func, NULL);
#endif
}

void update_progress(int percentage, int progress_id) {
    if (g_no_progress) return;
    (void)progress_id;
    if (percentage < 0) g_progress_percentage = 0;
    else if (percentage > 100) g_progress_percentage = 100;
    else g_progress_percentage = percentage;
}

void stop_progress(int progress_id) {
    (void)progress_id;
    if (!(g_flags & CONSOLE_FLAG_PROGRESS_RUNNING)) return;
    g_flags &= ~CONSOLE_FLAG_PROGRESS_RUNNING;
#ifdef CONSOLE_USE_RAW_CLONE
    console_thread_join(&g_progress_tid);
#else
    pthread_join(g_progress_thread, NULL);
#endif
}

#endif // CONSOLE_PROGRESS_IMPLEMENTATION
#endif // CONSOLE_PROGRESS_H
