#ifndef MAINTENANCE_H
#define MAINTENANCE_H

/*
 * maintenance.h - System maintenance operations
 *
 * verify_system : check installed files against filesystem
 * clean_cache   : remove cached/staging files
 */

int verify_system(void);
int clean_cache(int all);

#endif /* MAINTENANCE_H */