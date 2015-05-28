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
#include "gtd-manager.h"
#include "gtd-window.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

typedef struct
{
  GtkCssProvider *provider;
  GSettings      *settings;
  GtdManager     *manager;

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
gtd_application__manager_notify_ready_cb (GtdManager *manager,
                                          GParamSpec *pspec,
                                          gpointer    user_data)
{
  if (!gtd_object_get_ready (GTD_OBJECT (manager)))
    g_application_mark_busy (G_APPLICATION (user_data));
  else
    g_application_unmark_busy (G_APPLICATION (user_data));
}

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
                                     "The Todo authors"), created_year);
    }
  else
    {
      copyright = g_strdup_printf (_("Copyright \xC2\xA9 %Id\xE2\x80\x93%Id "
                                     "The Todo authors"), created_year, g_date_time_get_year (date));
    }

  gtk_show_about_dialog (GTK_WINDOW (priv->window),
                         "program-name", _("To Do"),
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
  g_set_application_name (_("To Do"));

  return g_object_new (GTD_TYPE_APPLICATION,
                       "application-id", "org.gnome.Todo",
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}

static void
gtd_application_activate (GApplication *application)
{
  GtdApplicationPrivate *priv = GTD_APPLICATION (application)->priv;

  if (!priv->provider)
   {
     GError *error = NULL;
     GFile* css_file;

     priv->provider = gtk_css_provider_new ();
     gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                GTK_STYLE_PROVIDER (priv->provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

     css_file = g_file_new_for_uri ("resource:///org/gnome/todo/theme/Adwaita.css");

     gtk_css_provider_load_from_file (priv->provider,
                                      css_file,
                                      &error);
     if (error != NULL)
       {
         g_warning ("%s: %s: %s",
                    G_STRFUNC,
                    _("Error loading CSS from resource"),
                    error->message);

         g_error_free (error);
       }
     else
       {
         g_object_unref (css_file);
       }
   }

  /* manager */
  if (!priv->manager)
    {
      priv->manager = gtd_manager_new ();

      g_signal_connect (priv->manager,
                        "notify::ready",
                        G_CALLBACK (gtd_application__manager_notify_ready_cb),
                        application);
    }

  /* window */
  if (priv->window == NULL)
    priv->window = gtd_window_new (GTD_APPLICATION (application));

  gtk_widget_show (priv->window);
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
gtd_application_startup (GApplication *application)
{
  /* app menu */
  g_application_set_resource_base_path (application, "/org/gnome/todo");

  /* add actions */
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   gtd_application_entries,
                                   G_N_ELEMENTS (gtd_application_entries),
                                   application);

  G_APPLICATION_CLASS (gtd_application_parent_class)->startup (application);
}

static void
gtd_application_class_init (GtdApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = gtd_application_finalize;

  application_class->activate = gtd_application_activate;
  application_class->startup = gtd_application_startup;
}

static void
gtd_application_init (GtdApplication *self)
{
  GtdApplicationPrivate *priv = gtd_application_get_instance_private (self);

  priv->settings = g_settings_new ("org.gnome.todo");

  self->priv = priv;
}

GtdManager*
gtd_application_get_manager (GtdApplication *app)
{
  g_return_val_if_fail (GTD_IS_APPLICATION (app), NULL);

  return app->priv->manager;
}
