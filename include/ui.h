#ifndef UI_H
#define UI_H

/*
 * ui.h - Flappy UX system
 *
 * Output philosophy: minimal, deterministic, honest, no noise.
 *
 * Status semantics:
 *   INFO  → state / progress
 *   WARN  → dangerous but allowed
 *   ERROR → operation failed
 *
 * Colour auto-detected via isatty(). Disabled in pipes/scripts.
 */

#include <stddef.h>

/* =====================
 * Status lines
 * ===================== */
void ui_info(const char *fmt, ...);
void ui_warn(const char *fmt, ...);
void ui_error(const char *fmt, ...);
void ui_ok(const char *fmt, ...);     /* ✔ <msg>  */
void ui_step(const char *fmt, ...);   /* plain step label, no prefix */

/* =====================
 * Progress bar (download only)
 *
 * ui_progress(done, total, speed_bytes)
 * Pass total=0 if unknown.
 * ===================== */
void ui_progress_init(const char *filename);
void ui_progress(double dlnow, double dltotal);
void ui_progress_finish(void);

/* curl XFERINFO callback — wire directly to CURLOPT_XFERINFOFUNCTION */
int ui_curl_progress_cb(void *clientp,
                        double dltotal,
                        double dlnow,
                        double ultotal,
                        double ulnow);

/* =====================
 * TTY detection
 * ===================== */
int ui_is_tty(void);

#endif /* UI_H */