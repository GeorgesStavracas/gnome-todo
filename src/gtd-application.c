/* gtd-application.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gtd-application.h"
#include "gtd-window.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>


typedef struct
{
  GSettings      *settings;

  GtkWidget      *window;
} GtdApplicationPrivate;

struct _GtdApplication
{
  GtkApplication         application;

  /*< private >*/
  GtdApplicationPrivate *priv;
};

static void           gtd_application_show_about                  (GSimpleAction        *simple,
                                                                   GVariant             *parameter,
                                                                   gpointer              user_data);

static void           gtd_application_quit                        (GSimpleAction        *simple,
                                                                   GVariant             *parameter,
                                                                   gpointer              user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GtdApplication, gtd_application, GTK_TYPE_APPLICATION)

static const GActionEntry gtd_application_entries[] = {
  /*{ "new",    gtd_application_create_new_list },*/
  { "about",  gtd_application_show_about },
  { "quit",   gtd_application_quit }
};

static void
gtd_application_show_about (GSimpleAction *simple,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  GtdApplicationPrivate *priv = GTD_APPLICATION (user_data)->priv;
  char *copyright;
  GDateTime *date;
  int created_year = 2015;

  date = g_date_time_new_now_local ();

  const gchar *authors[] = {
    "Georges Basile Stavracas Neto <georges.stavracas@gmail.com>",
    NULL
  };

  const gchar *artists[] = {
    "Allan Day <allanpday@gmail.com>",
    NULL
  };

  if (g_date_time_get_year (date) == created_year)
    {
      copyright = g_strdup_printf (_("Copyright \xC2\xA9 %Id "
                                     "The Todo authors"),
                                   created_year);
    }
  else
    {
      copyright = g_strdup_printf (_("Copyright \xC2\xA9 %Id\xE2\x80\x93%Id "
                                     "The Todo authors"),
                                   created_year, g_date_time_get_year (date));
    }

  gtk_show_about_dialog (GTK_WINDOW (priv->window),
                         "program-name", _("Todo"),
                         "version", VERSION,
                         "copyright", copyright,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "authors", authors,
                         "artists", artists,
                         "logo-icon-name", "gnome-todo",
                         "translator-credits", _("translator-credits"),
                         NULL);
  g_free (copyright);
  g_date_time_unref (date);
}

static void
gtd_application_quit (GSimpleAction *simple,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  GtdApplicationPrivate *priv = GTD_APPLICATION (user_data)->priv;

  gtk_widget_destroy (priv->window);
}

GtdApplication *
gtd_application_new (void)
{
  g_set_application_name ("Todo");

  return g_object_new (GTD_TYPE_APPLICATION,
                       "application-id", "org.gnome.Todo",
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}

static void
gtd_application_activate (GApplication *application)
{
  GtdApplicationPrivate *priv = GTD_APPLICATION (application)->priv;
  GtkBuilder *builder;
  GMenuModel *appmenu;

  builder = gtk_builder_new_from_resource ("/org/gnome/todo/ui/menus.ui");

  /* window */
  if (priv->window == NULL)
    priv->window = gtd_window_new (GTD_APPLICATION (application));

  gtk_widget_show (priv->window);

  /* app menu */
  appmenu = (GMenuModel*) gtk_builder_get_object (builder, "appmenu");

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   gtd_application_entries,
                                   G_N_ELEMENTS (gtd_application_entries),
                                   application);

  gtk_application_set_app_menu (GTK_APPLICATION (application), appmenu);

  g_object_unref (builder);
}

static void
gtd_application_finalize (GObject *object)
{
  GtdApplication *self = GTD_APPLICATION (object);

  /* Clear settings */
  g_clear_object (&(self->priv->settings));

  G_OBJECT_CLASS (gtd_application_parent_class)->finalize (object);
}

static void
gtd_application_class_init (GtdApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = gtd_application_finalize;

  application_class->activate = gtd_application_activate;
}

static void
gtd_application_init (GtdApplication *self)
{
  GtdApplicationPrivate *priv = gtd_application_get_instance_private (self);

  priv->settings = g_settings_new ("org.gnome.todo");

  self->priv = priv;
}
