-include ../config.mk

SUBDIRS +=	flappy
SUBDIRS +=	flappy-alternatives
SUBDIRS +=	flappy-create
SUBDIRS +=	flappy-dgraph
SUBDIRS +=	flappy-install
SUBDIRS +=	flappy-pkgdb
SUBDIRS +=	flappy-query
SUBDIRS +=	flappy-reconfigure
SUBDIRS +=	flappy-remove
SUBDIRS +=	flappy-rindex
SUBDIRS +=	flappy-uhelper
SUBDIRS +=	flappy-checkvers
SUBDIRS +=	flappy-fbulk
SUBDIRS +=	flappy-digest
SUBDIRS +=	flappy-fetch

ifeq (${FLAPPY_OS},linux)
SUBDIRS +=	flappy-uchroot
SUBDIRS +=	flappy-uunshare
endif

include ../mk/subdir.mk
