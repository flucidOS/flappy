# Flappy — Design Philosophy

This document explains the decisions behind Flappy's design. Understanding why something was built a certain way is as important as understanding what it does.

---

## The Core Constraint

Flappy is designed to be maintained by a single person for approximately two years.

This is not a disclaimer. It is the primary design constraint. Every architectural decision — what to include, what to exclude, how to handle errors, how to structure the database — was made with this constraint in mind.

A package manager that requires a team to maintain is not a minimal package manager. It is a team project that happens to manage packages.

---

## Why SQLite

SQLite was chosen for three reasons:

**Reliability.** SQLite is one of the most tested pieces of software in existence. It handles transaction atomicity, constraint enforcement, and crash recovery correctly. Writing equivalent behaviour from scratch would take months and introduce bugs that SQLite has not had for decades.

**Small footprint.** A single file on disk. No daemon. No network socket. No configuration. It opens, you query it, it closes.

**Easy inspection.** Any engineer with `sqlite3` installed can inspect the full state of the installed package database without any flappy-specific tooling. This is important: the database is the ground truth of the system state, and it must be readable by anyone.

---

## Why HTTPS + SHA256, Not Package Signing

Package signing with GPG or similar requires:

- Key generation and storage
- Key distribution and trust establishment
- Key rotation policy
- Revocation infrastructure

For a two-year project hosted on GitHub, this infrastructure adds maintenance cost without proportional security benefit. The GitHub HTTPS transport provides integrity in transit. The SHA256 checksum stored in `repo.db` provides integrity of the package itself.

This is a deliberate trade-off. It is not suitable for production systems managing critical infrastructure. It is appropriate for FlucidOS.

---

## Why No Post-Install Scripts

Post-install scripts are where package managers become operating systems.

Once you allow arbitrary code execution during installation, you must:

- Define a scripting environment
- Handle script failures
- Decide what permissions scripts have
- Test every script in every package
- Maintain the script execution framework indefinitely

Flappy installs files. The system configuration that depends on those files is the responsibility of the system, not the package manager.

---

## Why No Optional Dependencies

Optional dependencies require the package manager to understand what "optional" means in context. Does the user want this feature? Does the system need it? Is it a build-time option or a runtime option?

These questions have no deterministic answers. Flappy only models hard dependencies: if package A depends on package B, B must be installed for A to function. Everything else is a build-time decision made by the package author.

---

## Atomicity

The install pipeline has a single invariant: **a failed install leaves the system unchanged.**

This is enforced through:

1. Download and verification before any system mutation
2. `BEGIN IMMEDIATE` SQLite transaction around all database writes
3. Files are copied from staging to the real filesystem only after all database records are written
4. On any failure after filesystem writes have begun: rollback the database transaction and delete all written files

The staging directory (`/var/cache/flappy/staging/`) holds the extracted package until the commit step succeeds. After commit, staging is removed.

This means `flappy clean` is always safe to run. If staging contains anything, it is either an in-progress install (do not run clean) or a leftover from a failed install (safe to clean).

---

## Dependency Model

Flappy uses a directed dependency graph stored in the `dependencies` table. Edges represent "package A requires package B to be installed."

Three invariants are enforced at install time:

1. No self-dependencies
2. All declared dependencies must already be installed
3. No cycles (verified via DFS before committing the new node)

This means Flappy does not resolve dependency chains. If you install package A which depends on B and C, you must install B and C first. This is intentional. Automatic dependency resolution introduces complexity proportional to the number of packages and their version constraints. Flappy has no version constraint solver, and does not need one.

---

## Removal Model

`remove` and `purge` are two distinct operations with a clear difference:

- `remove` deletes everything except files under `/etc`. Config files are the user's responsibility.
- `purge` deletes everything including config files.

`purge --force` overrides the reverse-dependency check. This is the "user can deliberately break the system" escape hatch. It is always logged at ERROR level. The system does not prevent the user from breaking it — it records what happened honestly.

`autoremove` removes packages that were installed as dependencies and are no longer needed by anything. It uses safe removal semantics (keeps config files).

---

## Output Philosophy

Every line of output from flappy means something. There is no decorative output.

Status semantics:

- `[INFO]` — state change or progress notification
- `[WARN]` — dangerous but allowed action is being taken
- `[ERROR]` — operation failed

Query commands (`list`, `files`, `search`, etc.) produce only data — no labels, no status lines, no decorative output. Their output is suitable for piping.

Mutation commands (`install`, `remove`, `purge`) produce step-by-step output so the operator knows exactly what is happening.

Colour output is disabled automatically when stdout is not a TTY. This means flappy is safe to use in scripts and CI pipelines without any flags.

---

## The verify Command

`verify` is read-only. It reports. It never fixes.

This is a hard rule. A verify command that automatically repairs inconsistencies is not a diagnostic tool — it is an installer that runs without asking. The operator must decide what to do with the information `verify` provides.

This also means that after `flappy clean --all`, `verify` will report missing files for any package whose cache was cleared. The correct response is `flappy remove <pkg>` followed by `flappy install <pkg>`. Flappy will not do this automatically.

---

## What Flappy Does Not Do

These features were considered and explicitly excluded:

| Feature | Reason excluded |
|---|---|
| Dependency resolution | Adds resolver complexity; no version constraint model |
| Post-install scripts | Arbitrary code execution; maintenance burden |
| Optional dependencies | No deterministic model for "optional" |
| Package signing | Infrastructure cost exceeds benefit for this scope |
| Interactive prompts | Non-deterministic; breaks scripting |
| Automatic upgrades | `upgrade` is a planner, not an executor |
| Hook system | Equivalent to post-install scripts |
| Rollback history | No undo stack; atomicity handles this |

---

## Source Layout Decisions

Each source file has exactly one responsibility. The install pipeline is split across eight files (`install_guard`, `install_lookup`, `install_download`, `install_verify`, `install_conflict`, `install_extract`, `install_commit`) precisely so that each step can be read, tested, and modified independently.

The `graph.c` module owns all dependency graph operations. The `remove.c` module owns all removal operations. Nothing else touches these concerns.

This makes the codebase navigable by someone reading it for the first time, which is a requirement for a project with a defined maintenance window and a single author.