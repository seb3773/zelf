// Minimal bridge for Exomizer progress globals used by vendor code.
// Defines global symbols expected by src/compressors/exo/{progress.c,exo_helper.c}.
// Also exposes exo_set_progress_cb() to allow the packer to set a UI callback.

#include <stddef.h>

// Global progress state (referenced by exomizer sources)
void (*g_exo_progress_cb)(int) = NULL;  // optional callback [0..100]
int g_exo_pass_index = 0;               // 0-based pass index
int g_exo_max_passes = 3;               // total passes (default)
int g_exo_last_global = 0;              // last emitted global percent

void exo_set_progress_cb(void (*cb)(int)) {
    g_exo_progress_cb = cb;
    // Reset last to allow monotonic progression from fresh
    g_exo_pass_index = 0;
    g_exo_last_global = 0;
}
