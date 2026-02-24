#ifndef GRAPH_H
#define GRAPH_H

/*
 * graph.h - Installed dependency graph interface
 *
 * This layer implements deterministic graph semantics over the
 * installed package database.
 *
 * Responsibilities:
 *   - Direct dependency queries
 *   - Reverse dependency queries
 *   - Orphan computation
 *   - Strict identifier validation and normalization
 *
 * This layer does NOT:
 *   - Perform installation logic (handled elsewhere)
 *   - Resolve transitive dependencies
 *   - Auto-install missing packages
 *
 * All graph operations assume:
 *   - Canonical lowercase package identifiers
 *   - Strict ASCII identifier policy: [a-z0-9+.-]
 *   - Snapshot transaction consistency
 */

#include <stddef.h>

/*
 * graph_add_package
 *
 * Inserts a package node and its direct dependency edges.
 * Edges reference package identity via integer primary keys.
 *
 * Parameters:
 *   name          - canonical package name (lowercase enforced)
 *   version       - package version string
 *   explicit_flag - 1 if explicitly installed, 0 if dependency
 *   depends       - array of dependency names (canonicalized)
 *   depends_count - number of dependency entries
 *
 * Returns:
 *   0 on success
 *   1 on validation or structural failure
 */
int graph_add_package(
    const char *name,
    const char *version,
    int explicit_flag,
    const char **depends,
    size_t depends_count
);

/*
 * graph_depends
 *
 * Prints direct dependencies of a package.
 *
 * Behavior:
 *   - Fails if package does not exist
 *   - Prints nothing if no direct dependencies
 *   - Output sorted alphabetically (BINARY collation)
 *
 * Returns:
 *   0 on success
 *   1 if package does not exist
 */
int graph_depends(const char *name);

/*
 * graph_rdepends
 *
 * Prints direct reverse dependencies of a package.
 *
 * Behavior:
 *   - Fails if package does not exist
 *   - Prints nothing if no reverse dependencies
 *   - Output sorted alphabetically (BINARY collation)
 *
 * Returns:
 *   0 on success
 *   1 if package does not exist
 */
int graph_rdepends(const char *name);

/*
 * graph_orphans
 *
 * Prints packages that:
 *   - Are not explicit (explicit = 0)
 *   - Have no reverse dependencies
 *
 * Behavior:
 *   - Empty result is valid (exit 0)
 *   - Output sorted alphabetically (BINARY collation)
 *
 * Returns:
 *   0 on success
 */
int graph_orphans(void);

#endif /* GRAPH_H */