# Flappy

**Package manager for FlucidOS**

Flappy is a minimal, deterministic package manager built for FlucidOS. It manages binary package installation, dependency tracking, repository synchronisation, and system integrity verification through a strict, auditable pipeline.

---

## Design Philosophy

Flappy is built under a specific set of constraints:

- **Single maintainer** — no infrastructure beyond GitHub Pages
- **Deterministic behaviour** — same input always produces the same output
- **No silent actions** — every mutation is printed, every forced action is logged
- **No auto-fix** — flappy diagnoses problems; the operator decides what to do


---

## Requirements

**Runtime dependencies:**

| Library | Purpose |
|---|---|
| `libsqlite3` | Installed package database and repository metadata |
| `libarchive` | Package archive extraction (tar + zstd) |
| `libcurl` | Package and repository downloads |
| `libssl` / `libcrypto` | SHA256 package integrity verification |
| `libbsd` | BSD compatibility utilities |

**Build dependencies:** `gcc`, `make`, `pkg-config`

---

## Building

```sh
make
```

Development build (with `FLAPPY_DEV` flag):

```sh
make dev
```

---

## Installation

```sh
sudo make install
```

This installs the binary to `/usr/bin/flappy`, creates the log file at `/var/log/flappy.log`, and initialises the package database at `/var/lib/flappy/flappy.db`.

---

## Quick Start

```sh
# Initialise the database (first run only)
sudo flappy --init-db

# Fetch repository metadata
sudo flappy update

# Search for a package
flappy search curl

# Install a package
sudo flappy install curl

# List installed packages
flappy list

# Remove a package
sudo flappy remove curl
```

---

## Commands

### Core

| Command | Description |
|---|---|
| `flappy help` | Show help |
| `flappy version` | Show version |
| `flappy --init-db` | Initialise installed database (run once after install) |

### Query

| Command | Description |
|---|---|
| `flappy list` | List all installed packages |
| `flappy info <pkg>` | Show package name, version, install type |
| `flappy files <pkg>` | List all files owned by a package |
| `flappy owns <path>` | Show which package owns a file |
| `flappy inspect <pkg>` | Show raw package metadata from archive |
| `flappy depends <pkg>` | Show direct dependencies |
| `flappy rdepends <pkg>` | Show packages that depend on this package |
| `flappy orphans` | List dependency packages with no dependents |

### Repository

| Command | Description |
|---|---|
| `flappy update [url]` | Download and validate repository metadata |
| `flappy search [term]` | Search repository packages by prefix |
| `flappy upgrade` | Show available upgrades (dry-run, does not install) |

### Installation

| Command | Description |
|---|---|
| `flappy install <pkg>` | Install a package from the repository |

### Removal

| Command | Description |
|---|---|
| `flappy remove <pkg>` | Remove package files, keep config files under `/etc` |
| `flappy purge <pkg>` | Remove all package files including config files |
| `flappy purge --force <pkg>` | Force removal even if dependents exist (logged) |
| `flappy autoremove` | Remove all orphaned dependency packages |

### Maintenance

| Command | Description |
|---|---|
| `flappy verify` | Check all installed files exist on disk |
| `flappy clean` | Remove staging directory contents |
| `flappy clean --all` | Remove staging directory and package cache |

---

## Repository Layout

Flappy repositories follow a fixed layout hosted on any static file server (GitHub Pages recommended):

```
repo/
├── repo.db           SQLite database of available packages
├── repo.db.sha256    SHA256 checksum of repo.db
└── packages/
    └── <name>-<version>.pkg.tar.zst
```

The default repository URL is set at compile time in `include/flappy.h`.

---

## Package Format

Packages are tar archives compressed with zstd:

```
<name>-<version>.pkg.tar.zst
```

Archive contents:

```
./.PKGINFO     Package metadata
./.FILES       List of installed file paths (relative, no leading slash)
./usr/...      Actual files to install
./etc/...
./var/...
./opt/...
```

### .PKGINFO fields

```
pkgname = hello
pkgver  = 2.12
pkgrel  = 1
arch    = x86_64
pkgdesc = GNU hello program
depend  = glibc
conflict= hello-git
```

---

## File System Layout

| Path | Purpose |
|---|---|
| `/var/lib/flappy/flappy.db` | Installed package database |
| `/var/lib/flappy/repo.db` | Repository metadata cache |
| `/var/cache/flappy/packages/` | Downloaded package cache |
| `/var/cache/flappy/staging/` | Extraction staging area |
| `/var/log/flappy.log` | Operation log |

---

## Security Model

Trust chain:

```
HTTPS (GitHub Pages transport)
        ↓
repo.db SHA256 verification
        ↓
package SHA256 verification
        ↓
path validation before extraction
        ↓
filesystem write
```

Flappy validates:

- Repository database integrity via SHA256 before accepting it
- Each package's SHA256 before extraction
- Every archive path before writing (no `..`, no absolute paths, no forbidden roots)

Forbidden installation roots: `/proc`, `/dev`, `/sys`, `/home`, `/root`

Allowed installation roots: `/usr`, `/etc`, `/var`, `/opt`

---

## Exit Codes

See [EXIT_CODES.md](EXIT_CODES.md) for the complete reference.

| Code | Meaning |
|---|---|
| `0` | Success |
| `1` | Operation failed |
| `2` | Invalid usage (missing arguments) |
| `127` | Unknown command |

---

## Logging

All operations are logged to `/var/log/flappy.log`. Forced removals and dependency violations are logged at `ERROR` level regardless of whether the operation succeeded.

Log format:

```
[INFO] flappy invoked
[INFO] install: committed hello 2.12 (48 files)
[ERROR] forced purge of openssl which is required by curl
```

---

## Database Schema

```sql
CREATE TABLE packages (
    id       INTEGER PRIMARY KEY,
    name     TEXT UNIQUE NOT NULL,
    version  TEXT NOT NULL,
    explicit INTEGER NOT NULL CHECK (explicit IN (0,1))
);

CREATE TABLE files (
    path       TEXT PRIMARY KEY,
    package_id INTEGER NOT NULL,
    FOREIGN KEY(package_id) REFERENCES packages(id) ON DELETE CASCADE
);

CREATE TABLE dependencies (
    package_id INTEGER NOT NULL,
    depends_on INTEGER NOT NULL,
    PRIMARY KEY(package_id, depends_on),
    FOREIGN KEY(package_id) REFERENCES packages(id) ON DELETE CASCADE,
    FOREIGN KEY(depends_on) REFERENCES packages(id) ON DELETE CASCADE
);
```

Schema version is stored in the `meta` table and validated on every open.

---

## Project Structure

```
flappy/
├── include/
│   ├── flappy.h        Core definitions, DB paths, version
│   ├── graph.h         Dependency graph engine
│   ├── install.h       Installer pipeline
│   ├── remove.h        Removal engine
│   ├── maintenance.h   Verify and clean
│   ├── repo.h          Repository layer
│   ├── ui.h            Terminal output system
│   ├── version.h       Version comparison
│   ├── pkg_meta.h      Package metadata struct
│   └── db_guard.h      SQLite error handling
└── src/
    ├── main.c           Entry point
    ├── cli.c            Command dispatcher
    ├── ui.c             Terminal UX (colour, progress bar)
    ├── log.c            File logging
    ├── db_runtime.c     DB open/close/validate
    ├── db_schema.c      DB initialisation
    ├── db_guard.c       SQLite error handler
    ├── graph.c          Dependency graph (add, query, orphans)
    ├── version.c        Version comparison
    ├── pkg_parser.c     .PKGINFO parser
    ├── pkg_reader.c     Archive metadata reader
    ├── install.c        Install orchestrator
    ├── install_guard.c  Root check
    ├── install_lookup.c Repo DB lookup
    ├── install_download.c curl download + progress
    ├── install_verify.c SHA256 verification
    ├── install_extract.c Archive extraction to staging
    ├── install_conflict.c File conflict detection
    ├── install_commit.c  Atomic DB commit + file copy
    ├── remove.c         Remove/purge/autoremove engine
    ├── verify.c         File existence verification
    ├── clean.c          Cache cleanup
    ├── repo_update.c    Repository download + validation
    ├── repo_search.c    Repository search
    └── repo_upgrade.c   Upgrade detection (dry-run)
```

---

## License

MIT — see [LICENSE](LICENSE)
