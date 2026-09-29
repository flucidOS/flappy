-include ../config.mk

PCFILE = libflappy.pc

.PHONY: all
all:
	sed -e "s|@@VERSION@@|${VERSION}|g" \
		-e "s|@@PREFIX@@|${PREFIX}|g" \
		-e "s|@@EPREFIX@@|${EPREFIX}|g" \
		${PCFILE}.in > ${PCFILE}

.PHONY: install
install:
	install -d $(DESTDIR)$(MANDIR)/man5
	install -m644 flappy.d.5 $(DESTDIR)$(MANDIR)/man5
	install -d $(DESTDIR)$(MANDIR)/man7
	install -m644 flappy.7 $(DESTDIR)$(MANDIR)/man7
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -m644 $(PCFILE) $(DESTDIR)$(PKGCONFIGDIR)
	install -d $(DESTDIR)/$(DBDIR)/keys
	install -d $(DESTDIR)/$(SHAREDIR)/flappy.d
	install -m644 repod-main.conf $(DESTDIR)/$(SHAREDIR)/flappy.d/00-repository-main.conf
	install -m644 flappy.conf $(DESTDIR)/$(SHAREDIR)/flappy.d/flappy.conf
	install -d $(DESTDIR)/$(SHAREDIR)/zsh/site-functions
	install -m644 _flappy* $(DESTDIR)/$(SHAREDIR)/zsh/site-functions
	install -d $(DESTDIR)/$(SHAREDIR)/bash-completion/completions
	install -m644 flappy.bash $(DESTDIR)/$(SHAREDIR)/bash-completion/completions/flappy
	for file in flappy-checkvers flappy-create flappy-dgraph flappy-install \
		flappy-pkgdb flappy-query flappy-reconfigure flappy-remove flappy-rindex; do \
		ln -sf flappy $(DESTDIR)/$(SHAREDIR)/bash-completion/completions/$$file; \
	done

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(PCFILE)
	-rm -f $(DESTDIR)$(SHAREDIR)/flappy.d/00-repository-main.conf
	-rm -f $(DESTDIR)$(SHAREDIR)/flappy.d/flappy.conf


.PHONY: clean
clean:
	-rm -f $(PCFILE)
