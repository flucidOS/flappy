#ifndef INSTALL_GUARD_H
#define INSTALL_GUARD_H

/*
 * install_guard.h
 *
 * System mutation guard layer for Trail-6.
 *
 * Responsibilities:
 *   - Enforce root privilege
 *   - Prevent parallel installs
 *   - Ensure required directories exist
 *
 * Design:
 *   - Fail hard
 *   - Minimal state
 *   - No dynamic allocation
 */

int install_require_root(void);
int install_acquire_lock(void);
void install_release_lock(void);
int install_prepare_directories(void);

#endif