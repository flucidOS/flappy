# Flappy — Exit Code Reference

Every flappy command exits with one of the following codes.

---

## Exit Codes

| Code | Name | Meaning |
|---|---|---|
| `0` | Success | The operation completed successfully |
| `1` | Failure | The operation failed (see stderr for reason) |
| `2` | Usage error | Missing or invalid arguments |
| `127` | Unknown command | The command name was not recognised |

---

## Per-Command Reference

### `flappy help`
| Exit | Condition |
|---|---|
| `0` | Always |

### `flappy version`
| Exit | Condition |
|---|---|
| `0` | Always |

### `flappy --init-db`
| Exit | Condition |
|---|---|
| `0` | Database initialised successfully |
| `1` | Cannot create `/var/lib/flappy/`, cannot open or write DB, cannot set permissions |

### `flappy list`
| Exit | Condition |
|---|---|
| `0` | Always (empty output if no packages installed) |
| `1` | Database open failed |

### `flappy info <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package found and printed |
| `1` | Package not installed |
| `2` | No package name provided |

### `flappy files <pkg>`
| Exit | Condition |
|---|---|
| `0` | Files found and printed |
| `1` | No files recorded for package |
| `2` | No package name provided |

### `flappy owns <path>`
| Exit | Condition |
|---|---|
| `0` | Owning package found and printed |
| `1` | File not owned by any package |
| `2` | No path provided |

### `flappy inspect <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package archive read and metadata printed |
| `1` | Cannot read archive or parse metadata |
| `2` | No package file path provided |

### `flappy depends <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package found (empty output if no dependencies) |
| `1` | Package not installed |
| `2` | No package name provided |

### `flappy rdepends <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package found (empty output if no reverse dependencies) |
| `1` | Package not installed |
| `2` | No package name provided |

### `flappy orphans`
| Exit | Condition |
|---|---|
| `0` | Always (prints `[INFO] no orphan packages` if none) |
| `1` | Database open failed |

### `flappy update [url]`
| Exit | Condition |
|---|---|
| `0` | Repository downloaded, verified, and installed |
| `1` | Download failed, checksum mismatch, schema invalid, or rename failed |

### `flappy search [term]`
| Exit | Condition |
|---|---|
| `0` | Search completed (empty output if no matches) |
| `1` | Repository database not available (run `flappy update`) |

### `flappy upgrade`
| Exit | Condition |
|---|---|
| `0` | Comparison completed (prints `[INFO] system is up to date` if none) |
| `1` | Repository or installed database not available |

### `flappy install <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package installed successfully |
| `1` | Not root, package not in repo, download failed, checksum mismatch, conflict detected, extraction failed, DB commit failed |
| `2` | No package name provided |

### `flappy remove <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package removed successfully |
| `1` | Package not installed, reverse dependencies exist, file deletion failed, DB record removal failed |
| `2` | No package name provided |

### `flappy purge <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package purged successfully |
| `1` | Package not installed, reverse dependencies exist (without `--force`), file deletion failed, DB record removal failed |
| `2` | No package name provided |

### `flappy purge --force <pkg>`
| Exit | Condition |
|---|---|
| `0` | Package force-purged (dependents may be broken) |
| `1` | Package not installed, file deletion failed, DB record removal failed |
| `2` | No package name provided |

Note: `purge --force` always logs at `[ERROR]` level in `/var/log/flappy.log`, even on success.

### `flappy autoremove`
| Exit | Condition |
|---|---|
| `0` | All orphans removed (prints `[INFO] no orphan packages` if none) |
| `1` | One or more orphans could not be removed |

### `flappy verify`
| Exit | Condition |
|---|---|
| `0` | All installed files exist and are regular files |
| `1` | One or more files missing, wrong type, or package has no files registered |

### `flappy clean`
| Exit | Condition |
|---|---|
| `0` | Staging directory cleaned |
| `1` | Errors during cleanup |

### `flappy clean --all`
| Exit | Condition |
|---|---|
| `0` | Staging directory and package cache cleaned |
| `1` | Errors during cleanup |

---

## Notes

**Exit code `1` always prints a reason to stderr.** Flappy never exits with `1` silently. If you see exit code `1` with no output, that is a bug.

**Exit code `0` from query commands does not mean data was found.** `flappy list` exits `0` even when no packages are installed. The absence of output is the answer.

**Exit code `1` from `purge --force` means the operation failed.** The force flag bypasses dependency checks, not filesystem or database errors. If the package files cannot be deleted or the DB record cannot be removed, the operation fails and the system is left in its original state.