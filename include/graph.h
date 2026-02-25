#ifndef GRAPH_H
#define GRAPH_H

/*
 * graph.h - Installed dependency graph engine
 *
 * This module provides deterministic operations over the
 * installed package dependency graph.
 *
 * Architectural guarantees:
 *
 *   - Canonical lowercase identifiers
 *   - Strict ASCII identifier validation
 *   - Snapshot isolation for read queries
 *   - BEGIN IMMEDIATE for install operations
 *   - Deterministic traversal order (BINARY)
 *   - Install-time cycle detection
 *   - No partial graph states ever committed
 *
 * The graph identity model is integer-based (package_id).
 * Labels (names) are presentation metadata only.
 */

#include <stddef.h>

/*
 * graph_add_package
 *
 * Install a new package node and its direct dependencies.
 *
 * Behavior:
 *   - Fails if package already exists
 *   - Fails if any dependency is missing
 *   - Fails on self-dependency
 *   - Fails if resulting graph would contain a cycle
 *   - Entire operation is atomic
 *
 * Returns:
 *   0 on success
 *   non-zero on failure
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
 * Print direct dependencies of an installed package.
 *
 * Deterministic alphabetical output.
 */
int graph_depends(const char *name);

/*
 * graph_rdepends
 *
 * Print direct reverse dependencies of an installed package.
 */
int graph_rdepends(const char *name);

/*
 * graph_orphans
 *
 * Print packages that:
 *   - Are not explicit
 *   - Have no reverse dependencies
 */
int graph_orphans(void);

#endif /* GRAPH_H */