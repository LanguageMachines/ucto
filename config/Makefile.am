config_DATA = textcat_normal.cfg textcat_alt.cfg

configdir = $(datadir)/$(PACKAGE)

EXTRA_DIST = $(config_DATA)

install-data-hook:
	rm -f $(DESTDIR)$(configdir)/textcat.cfg
if OLD_LM
	$(LN_S) $(DESTDIR)$(configdir)/textcat_alt.cfg $(DESTDIR)$(configdir)/textcat.cfg
else
	$(LN_S) $(DESTDIR)$(configdir)/textcat_normal.cfg $(DESTDIR)$(configdir)/textcat.cfg
endif
