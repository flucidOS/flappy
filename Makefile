# Flappy - A utility application
# Copyright notice and license should be included here
# This Makefile builds the project with support for both development and production targets

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

# Installation paths
PREFIX  := /usr
BINDIR  := $(PREFIX)/bin
LOGFILE := /var/log/flappy.log

# Compiler flags: C11 standard, all warnings enabled, treat warnings as errors
CFLAGS  := -std=c11 -Wall -Wextra -Werror -I$(INC_DIR)
LDFLAGS :=

# External dependencies required by the project
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
	$(SRC_DIR)/cmd_inspect.c 

# Object files derived from source files
OBJS := $(SRCS:.c=.o)

# Default target: verify dependencies and build production binary
all: check-deps $(PROD_BIN)

# Verify that required tools and libraries are available on the system
check-deps:
	@command -v $(PKGCONF) >/dev/null || { echo "pkg-config missing"; exit 1; }
	@for lib in $(REQUIRED_LIBS); do \
		$(PKGCONF) --exists $$lib || { echo "Missing $$lib"; exit 1; }; \
	done

# Production binary target: link object files to create executable
$(PROD_BIN): $(OBJS)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) $^ -o $@ $(PKG_LIBS)

# Development target: build with debug symbols enabled
dev: CFLAGS += -DFLAPPY_DEV
dev: check-deps $(DEV_BIN)

# Development binary target: link object files with debug flags
$(DEV_BIN): $(OBJS)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) $^ -o $@ $(PKG_LIBS)

# Compilation rule: convert C source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -c $< -o $@

# Installation target: copy binary to system location (requires root)
install: all
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Error: installation requires root privileges"; exit 1; fi
	@echo "Installing $(PROD_BIN) to $(BINDIR)"
	@install -m 0755 $(PROD_BIN) $(BINDIR)/$(PROD_BIN)
	@echo "Creating log file $(LOGFILE)"
	@touch $(LOGFILE)
	@chmod 0600 $(LOGFILE)
	@chown root:root $(LOGFILE)
	@echo "Initializing database..."
	@$(BINDIR)/$(PROD_BIN) --init-db


# Clean target: remove generated artifacts and binaries
clean:
	rm -f $(OBJS) $(PROD_BIN) $(DEV_BIN)

# Declare phony targets that don't represent actual files
.PHONY: all dev install clean check-deps
