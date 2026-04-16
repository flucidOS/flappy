#ifndef HOOKS_H
#define HOOKS_H

#include <stddef.h>

#include <stddef.h>

/*
 * hooks.h - Package hook execution
 *
 * Hooks are bash functions stored in a .install file embedded in the
 * package archive as .INSTALL.  At install time the script is extracted
 * to /var/lib/flappy/hooks/<name>.install and kept there until the
 * package is removed.
 *
 * Supported functions (same interface as pacman):
 *
 *   pre_install()              called before files are written
 *   post_install()             called after files are written
 *   pre_upgrade(old_ver)       called before files are written
 *   post_upgrade(new_ver, old) called after files are written
 *   pre_remove(ver)            called before files are deleted
 *   post_remove(ver)           called after files are deleted
 *
 * Missing functions are silently skipped — a package that declares
 * only post_install() does not need to define pre_install().
 *
 * run_hook returns:
 *   0   function ran successfully or was not defined
 *   1   function was defined and exited non-zero
 */

#define FLAPPY_HOOKS_DIR "/var/lib/flappy/hooks"

/*
 * run_hook
 *
 * Sources `script_path` in bash and calls `function` with up to two
 * optional arguments (pass NULL for unused args).
 *
 * If the function is not defined in the script, returns 0 silently.
 */
int run_hook(const char *script_path,
             const char *function,
             const char *arg1,
             const char *arg2);

/*
 * hook_path
 *
 * Writes the canonical hook file path for `pkgname` into `out`.
 * `out` must be at least 256 bytes.
 */
void hook_path(const char *pkgname, char *out, size_t outsz);

/*
 * hook_install_from_pkg
 *
 * Extracts the .INSTALL script from the package archive at `pkgfile`
 * and writes it to FLAPPY_HOOKS_DIR/<pkgname>.install.
 *
 * Returns:
 *   0   script extracted successfully
 *   0   package has no .INSTALL (not an error)
 *  -1   archive could not be read
 *   1   write failed
 */
int hook_install_from_pkg(const char *pkgname, const char *pkgfile);

/*
 * hook_remove
 *
 * Deletes the hook file for `pkgname` if it exists.
 */
void hook_remove(const char *pkgname);

#endif /* HOOKS_H */