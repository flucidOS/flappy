#include "flappy.h"
#include "db_guard.h"
#include "ui.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static const char *SCHEMA_SQL =
"PRAGMA foreign_keys = ON;"
"BEGIN;"
"CREATE TABLE IF NOT EXISTS meta ("
"  schema_version INTEGER NOT NULL"
");"
"DELETE FROM meta;"
"INSERT INTO meta(schema_version) VALUES (2);"
"CREATE TABLE IF NOT EXISTS packages ("
"  id INTEGER PRIMARY KEY,"
"  name TEXT UNIQUE NOT NULL,"
"  version TEXT NOT NULL,"
"  explicit INTEGER NOT NULL CHECK (explicit IN (0,1))"
");"
"CREATE TABLE IF NOT EXISTS files ("
"  path TEXT PRIMARY KEY,"
"  package_id INTEGER NOT NULL,"
"  FOREIGN KEY(package_id) REFERENCES packages(id) ON DELETE CASCADE"
");"
"CREATE TABLE IF NOT EXISTS dependencies ("
"  package_id INTEGER NOT NULL,"
"  depends_on INTEGER NOT NULL,"
"  PRIMARY KEY(package_id, depends_on),"
"  FOREIGN KEY(package_id) REFERENCES packages(id) ON DELETE CASCADE,"
"  FOREIGN KEY(depends_on) REFERENCES packages(id) ON DELETE CASCADE"
");"
"COMMIT;";

static void mkdir_or_die(const char *p) {
    if (mkdir(p, 0755) == -1 && errno != EEXIST) {
        log_error("mkdir failed: %s: %s", p, strerror(errno));
        fprintf(stderr, "Fatal: cannot create %s\n", p);
        exit(1);
    }
}

int db_bootstrap_install(void) {
    sqlite3 *db = NULL;
    int rc;

    mkdir_or_die(FLAPPY_DB_DIR);

    rc = sqlite3_open(FLAPPY_DB_PATH, &db);
    if (rc != SQLITE_OK) db_die(db, rc, "open");

    rc = sqlite3_exec(db, SCHEMA_SQL, NULL, NULL, NULL);
    if (rc != SQLITE_OK) db_die(db, rc, "schema");

    sqlite3_close(db);

    if (chown(FLAPPY_DB_PATH, 0, 0) != 0 || chmod(FLAPPY_DB_PATH, 0600) != 0) {
        log_error("permission set failed on db");
        fprintf(stderr, "Fatal: cannot secure database file\n");
        exit(1);
    }

    log_info("database initialized (schema v%d)", FLAPPY_SCHEMA_VERSION);
    ui_info("database initialized");
    return 0;
}