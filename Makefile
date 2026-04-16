# Flappy - A utility application

# Compiler and tool configuration
CC      := gcc
PKGCONF := pkg-config

# Project naming
PROJECT := flappy
DEV_BIN := flappy-dev
PROD_BIN:= flappy

# Directory structure
SRC_DIR := src
INC_DIR := include

# Installation paths (DESTDIR supported for packaging)
PREFIX  ?= /usr
BINDIR  := $(PREFIX)/bin
MANDIR  := $(PREFIX)/share/man/man1
LOGFILE := /var/log/flappy.log

# Compiler flags
CFLAGS  := -std=c11 -Wall -Wextra -Werror -I$(INC_DIR)
LDFLAGS := -lssl -lcrypto

# External dependencies
REQUIRED_LIBS := libbsd sqlite3 libarchive libcurl
PKG_CFLAGS := $(shell $(PKGCONF) --cflags $(REQUIRED_LIBS))
PKG_LIBS   := $(shell $(PKGCONF) --libs $(REQUIRED_LIBS))

# Source files
SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/cli.c \
	$(SRC_DIR)/log.c \
	$(SRC_DIR)/db_guard.c \
	$(SRC_DIR)/db_schema.c \
	$(SRC_DIR)/db_runtime.c \
	$(SRC_DIR)/cmd_help.c \
	$(SRC_DIR)/cmd_version.c \
	$(SRC_DIR)/cmd_list.c \
	$(SRC_DIR)/cmd_info.c \
	$(SRC_DIR)/cmd_files.c \
	$(SRC_DIR)/cmd_owns.c \
	$(SRC_DIR)/pkg_reader.c \
	$(SRC_DIR)/pkg_parser.c \
	$(SRC_DIR)/cmd_inspect.c \
	$(SRC_DIR)/cmd_depends.c \
	$(SRC_DIR)/cmd_rdepends.c \
	$(SRC_DIR)/cmd_orphans.c \
	$(SRC_DIR)/graph.c \
	$(SRC_DIR)/version.c \
	$(SRC_DIR)/repo_update.c \
	$(SRC_DIR)/repo_search.c \
	$(SRC_DIR)/repo_upgrade.c \
	$(SRC_DIR)/install_guard.c \
	$(SRC_DIR)/install.c \
	$(SRC_DIR)/cmd_install.c \
	$(SRC_DIR)/install_download.c \
	$(SRC_DIR)/install_lookup.c \
	$(SRC_DIR)/install_verify.c \
	$(SRC_DIR)/install_extract.c \
	$(SRC_DIR)/install_commit.c \
	$(SRC_DIR)/install_conflict.c \
	$(SRC_DIR)/resolve.c \
	$(SRC_DIR)/remove.c \
	$(SRC_DIR)/cmd_remove.c \
	$(SRC_DIR)/cmd_purge.c \
	$(SRC_DIR)/cmd_autoremove.c \
	$(SRC_DIR)/verify.c \
	$(SRC_DIR)/clean.c \
	$(SRC_DIR)/cmd_verify.c \
	$(SRC_DIR)/cmd_clean.c \
	$(SRC_DIR)/ui.c \
	$(SRC_DIR)/env.c \
	$(SRC_DIR)/install_constraints.c \
	$(SRC_DIR)/sha256.c \
	$(SRC_DIR)/hooks.c

# Object files
OBJS := $(SRCS:.c=.o)

# Default target
all: check-deps $(PROD_BIN)

# Dependency check
check-deps:
	@command -v $(PKGCONF) >/dev/null || { echo "pkg-config missing"; exit 1; }
	@for lib in $(REQUIRED_LIBS); do \
		$(PKGCONF) --exists $$lib || { echo "Missing $$lib"; exit 1; }; \
	done

# Build production binary
$(PROD_BIN): $(OBJS)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) $^ -o $@ $(PKG_LIBS) $(LDFLAGS)

# Development build
dev: CFLAGS += -DFLAPPY_DEV
dev: check-deps $(DEV_BIN)

$(DEV_BIN): $(OBJS)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) $^ -o $@ $(PKG_LIBS) $(LDFLAGS)

# Compile rule
%.o: %.c
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -c $< -o $@

# Install (clean, idempotent, packaging-safe)
install: all
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Error: installation requires root privileges"; exit 1; fi

	@echo "Installing binary..."
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 0755 $(PROD_BIN) $(DESTDIR)$(BINDIR)/$(PROD_BIN)

	@echo "Installing man pages..."
	@install -d $(DESTDIR)$(MANDIR)
	@install -m 0644 man/flappy.1 $(DESTDIR)$(MANDIR)/
	@install -m 0644 man/flappy-install.1 $(DESTDIR)$(MANDIR)/
	@install -m 0644 man/flappy-remove.1 $(DESTDIR)$(MANDIR)/
	@install -m 0644 man/flappy-verify.1 $(DESTDIR)$(MANDIR)/

	@if [ -z "$(DESTDIR)" ]; then \
		echo "Updating man database..."; \
		mandb; \
	fi

	@echo "Creating log file if not exists..."
	@if [ ! -f $(LOGFILE) ]; then \
		touch $(LOGFILE); \
		chmod 0600 $(LOGFILE); \
		chown root:root $(LOGFILE); \
	fi

	@echo "Install complete."

# Uninstall target
uninstall:
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Error: uninstall requires root privileges"; exit 1; fi

	@echo "Removing binary..."
	@rm -f $(BINDIR)/$(PROD_BIN)

	@echo "Removing man pages..."
	@rm -f $(MANDIR)/flappy*.1

	@echo "Uninstall complete."

# Clean
clean:
	rm -f $(OBJS) $(PROD_BIN) $(DEV_BIN)

.PHONY: all dev install uninstall clean check-deps