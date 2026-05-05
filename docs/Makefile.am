man1_MANS = ucto.1

EXTRA_DIST = ucto.1

# https://stackoverflow.com/questions/10682603/generating-and-installing-doxygen-documentation-with-autotools

if HAVE_DOXYGEN
directory = $(top_srcdir)/docs/html

$(directory): doxyfile.stamp

doxyfile.stamp: dox.cfg
	$(DOXYGEN) $^
	echo Timestamp > $@

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp
clean-local:
	-rm -rf $(top_srcdir)/docs/html
endif
