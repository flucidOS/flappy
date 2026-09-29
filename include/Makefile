-include ../config.mk

INCS =	flappy.h

.PHONY: all
all:
	sed -e "s|@@VERSION@@|${VERSION}|g" ${INCS}.in > ${INCS}

.PHONY: install
install:
	install -d $(DESTDIR)$(INCLUDEDIR)/flappy
	install -m 644 $(INCS) $(DESTDIR)$(INCLUDEDIR)
	for f in array bool data dictionary number object string; do \
		install -m 644 flappy/flappy_$${f}.h $(DESTDIR)$(INCLUDEDIR)/flappy; \
	done

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(INCLUDEDIR)/$(INCS)

.PHONY: clean
clean:
	-rm -f ${INCS}
