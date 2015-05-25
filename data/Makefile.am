desktopdir = $(datadir)/applications
desktop_in_files = org.gnome.Todo.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.ini: %.ini.in
	LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = $(srcdir)/appdata/org.gnome.Todo.appdata.xml.in

@INTLTOOL_DESKTOP_RULE@

@GSETTINGS_RULES@

%.gschema.xml: %.gschema.xml.in
	$(AM_V_GEN) $(INTLTOOL_MERGE) -x -u --no-translations $< $@

gsettingsschema_in_files = org.gnome.todo.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

EXTRA_DIST=                     \
  appdata/org.gnome.Todo.appdata.xml \
  org.gnome.Todo.desktop \
  todo.gresource.xml \
  ui/edit-pane.ui \
  ui/list-view.ui \
  ui/menus.ui \
  ui/task-list-item.ui \
  ui/task-row.ui \
  ui/window.ui \
  theme/Adwaita.css \
  theme/bg.png \
  theme/bg.svg \
  $(appdata_in_files) \
  $(desktop_in_files) \
  $(gsettingsschema_in_files)

CLEANFILES =                    \
  $(service_DATA)               \
  $(desktop_DATA)               \
  $(searchprovider_DATA)        \
  $(gsettings_SCHEMAS)          \
  $(appdata_DATA)

-include $(top_srcdir)/git.mk
