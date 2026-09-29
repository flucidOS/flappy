TOPDIR = ../..
-include $(TOPDIR)/config.mk

SCRIPT = flappy
MAN = flappy.1

.PHONY: all
all: $(SCRIPT)

$(SCRIPT): flappy.sh
	cp flappy.sh $(SCRIPT)
	chmod 755 $(SCRIPT)

.PHONY: install
install: all
	install -d $(DESTDIR)$(SBINDIR)
	install -m 755 $(SCRIPT) $(DESTDIR)$(SBINDIR)
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 $(MAN) $(DESTDIR)$(MANDIR)/man1

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(SBINDIR)/$(SCRIPT)
	-rm -f $(DESTDIR)$(MANDIR)/man1/$(MAN)

.PHONY: clean
clean:
	-rm -f $(SCRIPT)
