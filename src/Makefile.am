AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\" \
	-DUI_DATA_DIR=\""$(pkgdatadir)/style"\" \
	$(GNOME_TODO_CFLAGS)

bin_PROGRAMS = gnome-todo

BUILT_SOURCES = \
    gtd-resources.c \
    gtd-resources.h

gnome_todo_SOURCES = \
	$(BUILT_SOURCES) \
	gtd-application.c \
	gtd-application.h \
	gtd-arrow-frame.c \
	gtd-arrow-frame.h \
	gtd-edit-pane.c \
	gtd-edit-pane.h \
	gtd-enums.h \
	gtd-list-view.c \
	gtd-list-view.h \
	gtd-manager.c \
	gtd-manager.h \
	gtd-object.c \
	gtd-object.h \
	gtd-task.c \
	gtd-task.h \
	gtd-task-list.c \
	gtd-task-list.h \
	gtd-task-list-item.c \
	gtd-task-list-item.h \
	gtd-task-row.c \
	gtd-task-row.h \
	gtd-types.h \
	gtd-window.c \
	gtd-window.h \
	main.c

gnome_todo_LDFLAGS = \
	-Wl,--export-dynamic

gnome_todo_LDADD = \
	-lm \
	$(GNOME_TODO_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(top_srcdir)/data --generate-dependencies $(top_srcdir)/data/todo.gresource.xml)
gtd-resources.c: $(top_srcdir)/data/todo.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-source --c-name todo $(top_srcdir)/data/todo.gresource.xml
gtd-resources.h: $(top_srcdir)/data/todo.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-header --c-name todo $(top_srcdir)/data/todo.gresource.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
