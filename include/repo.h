#ifndef REPO_H
#define REPO_H

#define FLAPPY_REPO_DIR        "/var/lib/flappy"
#define FLAPPY_REPO_DB_PATH    "/var/lib/flappy/repo.db"
#define FLAPPY_REPO_TMP_PATH   "/var/lib/flappy/repo.db.tmp"
#define FLAPPY_REPO_SHA_PATH   "/var/lib/flappy/repo.db.sha256"
#define FLAPPY_REPO_SCHEMA_VERSION 1

/*
 * repo.h - Repository metadata layer (Trail-5)
 *
 * This module handles:
 *   - Repository DB download
 *   - Repository DB validation
 *   - Metadata search
 *   - Upgrade inspection (dry-run)
 *
 * This layer does NOT:
 *   - Install packages
 *   - Resolve dependencies
 *   - Modify installed DB
 *
 * Repository DB is fully separate from installed DB.
 */

/*
 * repo_update
 *
 * Downloads repository database and validates it.
 *
 * Requires root privileges.
 *
 * Returns:
 *   0 on success
 *   non-zero on failure
 */
int repo_update(const char *url);

/*
 * repo_search
 *
 * Search repository database.
 *
 * If term == NULL, prints all packages.
 *
 * Returns:
 *   0 on success
 *   non-zero on failure
 */
int repo_search(const char *term);

/*
 * repo_upgrade
 *
 * Compare installed packages against repository metadata.
 * Prints available upgrades (dry-run only).
 *
 * Returns:
 *   0 on success
 *   non-zero on failure
 */
int repo_upgrade(void);

int repo_install(const char *repo);

#endif /* REPO_H */