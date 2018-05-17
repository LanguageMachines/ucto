config_DATA = textcat_normal.cfg textcat_alt.cfg

configdir = $(datadir)/$(PACKAGE)

EXTRA_DIST = $(config_DATA)

install-data-hook:
	rm -f $(configdir)/textcat.cfg
if OLD_LM
	$(LN_S) $(configdir)/textcat_alt.cfg $(configdir)/textcat.cfg
else
	$(LN_S) $(configdir)/textcat_normal.cfg $(configdir)/textcat.cfg
endif
