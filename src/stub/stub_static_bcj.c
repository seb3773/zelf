// BCJ-only static stub wrapper: include the legacy minimal BCJ core
// This reproduces the exact historical BCJ stub size/behavior
// Force using the legacy prototype-only BCJ header to keep decoder out-of-line
#include "bcj_x86_min_legacy.h"
#include "stub_core_static.inc"
