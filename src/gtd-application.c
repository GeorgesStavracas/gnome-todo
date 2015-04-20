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

G_DEFINE_TYPE_WITH_PRIVATE (GtdApplication, gtd_application, GTK_TYPE_APPLICATION)

GtdApplication *
gtd_application_new (void)
{
  g_set_application_name ("Todo");

  return g_object_new (GTD_TYPE_APPLICATION,
                       "application-id", "org.gnome.Todo",
                       "flags", G_APPLICATION_FLAGS_NONE,
                       "inactivity-timeout", 30000,
                       NULL);
}

static void
gtd_application_activate (GApplication *application)
{
  GtdApplicationPrivate *priv = GTD_APPLICATION (application)->priv;

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
