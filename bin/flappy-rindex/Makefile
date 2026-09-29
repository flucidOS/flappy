TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	flappy-rindex
OBJS =	main.o index-add.o index-clean.o remove-obsoletes.o repoflush.o sign.o

include $(TOPDIR)/mk/prog.mk

LDFLAGS += -lcrypto
