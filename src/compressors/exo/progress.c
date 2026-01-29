/*
 * Copyright (c) 2005 Magnus Lind.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software in a
 *   product, an acknowledgment in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not
 *   be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any distribution.
 *
 */

#include "progress.h"
#include "log.h"
#include <string.h>

/* External progress bridge (defined in EXO wrapper) */
extern void (*g_exo_progress_cb)(int);
extern int g_exo_pass_index;   /* 0-based */
extern int g_exo_max_passes;   /* >=1 */
extern int g_exo_last_global;  /* monotonic guard */

#define BAR_LENGTH 64

void progress_init(struct progress *p, char *msg, int start, int end)
{
    if(start > end)
    {
        p->factor = (float)BAR_LENGTH / (end - start);
        p->offset = -start;
    }
    else
    {
        p->factor = (float)BAR_LENGTH / (start - end);
        p->offset = start;
    }
    p->last = -1;
    if(msg == NULL)
    {
        msg = "progressing_";
    }
    p->msg = msg;

    LOG(LOG_DEBUG, ("start %d, end %d, pfactor %f, poffset %d\n",
                    start, end, p->factor, p->offset));
}

void progress_bump(struct progress *p, int pos)
{
    int fraction = ((pos + p->offset) * p->factor) + 0.5;
    while(fraction > p->last)
    {
        if(p->last == -1)
        {
            LOG_TTY(LOG_NORMAL, ("  %*s]\r [", BAR_LENGTH, ""));
        }
        else
        {
            LOG_TTY(LOG_NORMAL, ("%c", p->msg[p->last % strlen(p->msg)]));
        }
        p->last += 1;
    }

    /* Fine-grained callback: map bar fill (0..BAR_LENGTH) into global 0..100 */
    if (g_exo_progress_cb) {
        int bar_fill = fraction; if (bar_fill < 0) bar_fill = 0; if (bar_fill > BAR_LENGTH) bar_fill = BAR_LENGTH;
        int mp = (g_exo_max_passes > 0) ? g_exo_max_passes : 3;
        int base = (g_exo_pass_index * 100) / mp;                 /* e.g., 0,33,66 */
        int next_base = ((g_exo_pass_index + 1) * 100) / mp;      /* e.g., 33,66,100 */
        int width = next_base - base;                              /* e.g., 33,33,34 */
        int pass_pct = (bar_fill * 100) / BAR_LENGTH;              /* 0..100 within pass */
        int global_pct = base + (pass_pct * width) / 100;          /* map to segment */
        if (global_pct > 99) global_pct = 99; /* 100% reserved for final */
        if (global_pct < g_exo_last_global) global_pct = g_exo_last_global; /* monotonic */
        if (global_pct < 0) global_pct = 0;
        g_exo_last_global = global_pct;
        g_exo_progress_cb(global_pct);
    }
}

void progress_free(struct progress *p)
{
    LOG_TTY(LOG_NORMAL, ("\n"));
}
