#ifndef REMOVE_H
#define REMOVE_H

/*
 * remove.h - Package removal engine
 *
 * remove_package   : safe removal, keeps /etc configs
 * purge_package    : full removal, force flag bypasses rdep check
 * autoremove_packages : remove all orphaned dependencies
 */

int remove_package(const char *name);
int purge_package(const char *name, int force);
int autoremove_packages(void);

#endif /* REMOVE_H */
