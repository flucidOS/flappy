/*
 * ui.c - Flappy UX system
 */

#define _POSIX_C_SOURCE 200809L

#include "ui.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define COL_RESET  "\033[0m"
#define COL_RED    "\033[31m"
#define COL_GREEN  "\033[32m"
#define COL_YELLOW "\033[33m"
#define COL_BOLD   "\033[1m"

/* =========================================================================
 * TTY detection
 * ========================================================================= */

int ui_is_tty(void)
{
    static int cached = -1;
    if (cached < 0)
        cached = isatty(STDERR_FILENO);
    return cached;
}

/* =========================================================================
 * Status output
 * ========================================================================= */

static void emit(const char *plain_prefix,
                 const char *colour_open,
                 const char *colour_prefix,
                 const char *colour_close,
                 const char *fmt,
                 va_list ap)
{
    if (ui_is_tty())
        fprintf(stderr, "%s%s%s ", colour_open, colour_prefix, colour_close);
    else
        fprintf(stderr, "%s ", plain_prefix);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void ui_info(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    emit("[INFO]", "", "[INFO]", "", fmt, ap);
    va_end(ap);
}

void ui_warn(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    emit("[WARN]", COL_YELLOW COL_BOLD, "[WARN]", COL_RESET, fmt, ap);
    va_end(ap);
}

void ui_error(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    emit("[ERROR]", COL_RED COL_BOLD, "[ERROR]", COL_RESET, fmt, ap);
    va_end(ap);
}

void ui_ok(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    emit("\xe2\x9c\x94", COL_GREEN COL_BOLD, "\xe2\x9c\x94", COL_RESET, fmt, ap);
    va_end(ap);
}

void ui_step(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* =========================================================================
 * Progress bar
 *
 * - Renders on stderr with \r so it overwrites in place
 * - Guarded: only renders when dltotal > 0 and values are sane
 * - Renders at most once per percentage point to avoid flooding
 * ========================================================================= */

#define BAR_WIDTH 36

static int progress_last_pct = -1;

void ui_progress_init(const char *filename)
{
    progress_last_pct = -1;
    fprintf(stderr, "downloading %s\n", filename);
}

void ui_progress(double dlnow, double dltotal)
{
    if (!ui_is_tty())
        return;

    /* Ignore bogus values from early curl callbacks.
     * Sanity check: reject anything over 10 GB or negative. */
    if (dltotal < 1.0 || dlnow < 0.0 || dlnow > dltotal)
        return;
    if (dltotal > 10.0 * 1024.0 * 1024.0 * 1024.0)
        return;

    int pct = (int)((dlnow / dltotal) * 100.0);

    /* Clamp to valid range */
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    /* Only redraw when percentage changes */
    if (pct == progress_last_pct)
        return;
    progress_last_pct = pct;

    int    filled = (pct * BAR_WIDTH) / 100;
    fprintf(stderr, "\r  [");
    for (int i = 0; i < BAR_WIDTH; i++) {
        if (i < filled)
            fprintf(stderr, "%s\xe2\x96\x88%s", COL_GREEN, COL_RESET);
        else
            fprintf(stderr, "\xe2\x96\x91");
    }

    /* Show size in appropriate unit */
    if (dltotal >= 1024.0 * 1024.0) {
        /* MB */
        double mb_now = dlnow   / (1024.0 * 1024.0);
        double mb_tot = dltotal / (1024.0 * 1024.0);
        fprintf(stderr, "] %3d%%  %.1f / %.1f MB", pct, mb_now, mb_tot);
    } else if (dltotal >= 1024.0) {
        /* KB */
        double kb_now = dlnow   / 1024.0;
        double kb_tot = dltotal / 1024.0;
        fprintf(stderr, "] %3d%%  %.0f / %.0f KB", pct, kb_now, kb_tot);
    } else {
        fprintf(stderr, "] %3d%%", pct);
    }
    fflush(stderr);
}

void ui_progress_finish(void)
{
    if (ui_is_tty()) {
        if (progress_last_pct >= 0) {
            /* Ensure bar shows 100% before finishing */
            if (progress_last_pct < 100) {
                fprintf(stderr, "\r  [");
                for (int i = 0; i < BAR_WIDTH; i++)
                    fprintf(stderr, "%s\xe2\x96\x88%s", COL_GREEN, COL_RESET);
                fprintf(stderr, "] 100%%");
                fflush(stderr);
            }
            fprintf(stderr, "\n");
        }
        ui_ok("download complete");
    } else {
        fprintf(stderr, "download complete\n");
    }
    progress_last_pct = -1;
}

int ui_curl_progress_cb(void *clientp,
                        double dltotal,
                        double dlnow,
                        double ultotal,
                        double ulnow)
{
    (void)clientp;
    (void)ultotal;
    (void)ulnow;

    ui_progress(dlnow, dltotal);
    return 0;
}