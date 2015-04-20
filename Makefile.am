ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = src data po
DIST_SUBDIRS = src data po

INTLTOOL_FILES = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

dist_doc_DATA = \
	README \
	COPYING \
	AUTHORS \
	ChangeLog \
	INSTALL \
	NEWS

EXTRA_DIST = \
	$(INTLTOOL_FILES)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-appstream-util

# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(docdir)

.PHONY: AUTHORS
