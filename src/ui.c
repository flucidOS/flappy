/*
 * ui.c - Flappy UX system (refined, stable, production-grade)
 */

#define _POSIX_C_SOURCE 200809L

#include "ui.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

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
 * Time
 * ========================================================================= */

static double now_seconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* =========================================================================
 * Core emitter
 * ========================================================================= */

static void emit(const char *prefix, const char *fmt, va_list ap)
{
    fprintf(stderr, "%s ", prefix);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

/* =========================================================================
 * Status output
 * ========================================================================= */

void ui_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit("[INFO]", fmt, ap);
    va_end(ap);
}

void ui_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit("[WARN]", fmt, ap);
    va_end(ap);
}

void ui_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit("[ERROR]", fmt, ap);
    va_end(ap);
}

void ui_ok(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit("[OK]", fmt, ap);
    va_end(ap);
}

void ui_step(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* =========================================================================
 * Size formatter
 * ========================================================================= */

static void format_size(double bytes, double *value, const char **unit)
{
    if (bytes >= 1024.0 * 1024.0 * 1024.0) {
        *value = bytes / (1024.0 * 1024.0 * 1024.0);
        *unit = "GB";
    } else if (bytes >= 1024.0 * 1024.0) {
        *value = bytes / (1024.0 * 1024.0);
        *unit = "MB";
    } else if (bytes >= 1024.0) {
        *value = bytes / 1024.0;
        *unit = "KB";
    } else {
        *value = bytes;
        *unit = "B";
    }
}

/* =========================================================================
 * Progress
 * ========================================================================= */

#define BAR_WIDTH 30

static int progress_last_pct = -1;
static double last_time = 0;
static double last_bytes = 0;
static double smooth_speed = 0;
static double start_time = 0;

void ui_progress(double dlnow, double dltotal)
{
    if (!ui_is_tty())
        return;

    if (dltotal < 1.0 || dlnow < 0 || dlnow > dltotal)
        return;

    int pct = (int)((dlnow / dltotal) * 100.0);
    if (pct == progress_last_pct)
        return;

    progress_last_pct = pct;

    double now = now_seconds();

    /* =============================
     * Speed calculation with delay
     * ============================= */
    double speed = 0;

    if (last_time > 0 && now > last_time) {
        double dt = now - last_time;
        double db = dlnow - last_bytes;

        double inst = db / dt;

        if (smooth_speed == 0)
            smooth_speed = inst;
        else
            smooth_speed = 0.85 * smooth_speed + 0.15 * inst;

        speed = smooth_speed;
    }

    last_time = now;
    last_bytes = dlnow;

    /* Ignore speed for first 0.5 sec */
    if (now - start_time < 0.5)
        speed = 0;

    /* =============================
     * Format sizes
     * ============================= */
    double nv, tv, sv;
    const char *nu, *tu, *su;

    format_size(dlnow, &nv, &nu);
    format_size(dltotal, &tv, &tu);
    format_size(speed, &sv, &su);

    /* =============================
     * ETA (only for large downloads)
     * ============================= */
    int show_eta = (dltotal > 5 * 1024 * 1024 && speed > 0);

    int m = 0, s = 0;
    if (show_eta) {
        double eta = (dltotal - dlnow) / speed;
        m = (int)eta / 60;
        s = (int)eta % 60;
    }

    /* =============================
     * Render bar (fixed width)
     * ============================= */
    int filled = (pct * BAR_WIDTH) / 100;

    fprintf(stderr, "\r[");

    for (int i = 0; i < BAR_WIDTH; i++) {
        if (i < filled)
            fputc('#', stderr);
        else
            fputc('-', stderr);
    }

    /* =============================
     * Print info
     * ============================= */
    if (show_eta) {
        fprintf(stderr,
            "] %3d%% %.1f %s / %.1f %s (%.1f %s/s %02d:%02d)",
            pct, nv, nu, tv, tu, sv, su, m, s);
    } else {
        fprintf(stderr,
            "] %3d%% %.1f %s / %.1f %s (%.1f %s/s)",
            pct, nv, nu, tv, tu, sv, su);
    }

    fflush(stderr);
}

/* =========================================================================
 * Progress lifecycle
 * ========================================================================= */

void ui_progress_init(const char *filename)
{
    progress_last_pct = -1;
    last_time = 0;
    last_bytes = 0;
    smooth_speed = 0;
    start_time = now_seconds();

    fprintf(stderr, "downloading %s\n", filename);
}

void ui_progress_finish(void)
{
    if (ui_is_tty()) {
        if (progress_last_pct >= 0)
            fprintf(stderr, "\n");

        ui_ok("download complete");
    } else {
        fprintf(stderr, "download complete\n");
    }

    progress_last_pct = -1;
}

/* =========================================================================
 * Curl callback
 * ========================================================================= */

int ui_curl_progress_cb(void *clientp,
                       curl_off_t dltotal,
                       curl_off_t dlnow,
                       curl_off_t ultotal,
                       curl_off_t ulnow)
{
    (void)clientp;
    (void)ultotal;
    (void)ulnow;

    ui_progress((double)dlnow, (double)dltotal);
    return 0;
}