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
#include "gtd-initial-setup-window.h"
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
  GtkWidget      *initial_setup;
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
run_window (GtdApplication *application)
{
  GtdApplicationPrivate *priv;

  g_return_if_fail (GTD_IS_APPLICATION (application));

  priv = application->priv;

  if (!priv->window)
    priv->window = gtd_window_new (GTD_APPLICATION (application));

  gtk_widget_show (priv->window);
}

static void
finish_initial_setup (GtdApplication *application)
{
  g_return_if_fail (GTD_IS_APPLICATION (application));

  run_window (application);

  g_settings_set_boolean (application->priv->settings,
                          "first-run",
                          FALSE);

  g_clear_pointer (&application->priv->initial_setup, gtk_widget_destroy);
}

static void
run_initial_setup (GtdApplication *application)
{
  GtdApplicationPrivate *priv;

  g_return_if_fail (GTD_IS_APPLICATION (application));

  priv = application->priv;

  if (!priv->initial_setup)
    {
      priv->initial_setup = gtd_initial_setup_window_new (application);

      g_signal_connect (priv->initial_setup,
                        "cancel",
                        G_CALLBACK (gtk_widget_destroy),
                        application);

      g_signal_connect_swapped (priv->initial_setup,
                                "done",
                                G_CALLBACK (finish_initial_setup),
                                application);
    }

  gtk_widget_show (priv->initial_setup);
}

static void
gtd_application_activate (GApplication *application)
{
  GtdApplicationPrivate *priv = GTD_APPLICATION (application)->priv;
  gboolean first_run;

  first_run = g_settings_get_boolean (priv->settings, "first-run");

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

  /* If it's the first run of To Do, it should run the initial setup window */
  if (first_run)
    run_initial_setup (GTD_APPLICATION (application));
  else
    run_window (GTD_APPLICATION (application));
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
  GtdApplicationPrivate *priv = GTD_APPLICATION (application)->priv;

  /* manager */
  priv->manager = gtd_manager_new ();

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

/**
 * gtd_application_get_storage_location:
 *
 * Retrieves the default storage location for new #GtdTaskList.
 *
 * Returns: (transfer full): a newly allocated string containing the default
 * storage location. "local" is the default.
 */
gchar*
gtd_application_get_storage_location (GtdApplication *app)
{
  g_return_val_if_fail (GTD_IS_APPLICATION (app), NULL);

  return g_settings_get_string (app->priv->settings, "storage-location");
}

/**
 * gtd_application_set_storage_location:
 *
 * Sets the default storage location for the application. New lists will
 * be created there by default.
 *
 * Returns:
 */
void
gtd_application_set_storage_location (GtdApplication *application,
                                      const gchar    *location)
{
  g_return_if_fail (GTD_IS_APPLICATION (application));

  g_settings_set_string (application->priv->settings,
                         "storage-location",
                         location);
}
