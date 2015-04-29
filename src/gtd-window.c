/* gtd-window.c
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

#include "gtd-application.h"
#include "gtd-manager.h"
#include "gtd-window.h"

#include <glib/gi18n.h>

typedef struct
{
  GtdManager                    *manager;
} GtdWindowPrivate;

struct _GtdWindow
{
  GtkApplicationWindow  application;

  /*< private >*/
  GtdWindowPrivate     *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdWindow, gtd_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_MANAGER,
  LAST_PROP
};

GtkWidget*
gtd_window_new (GtdApplication *application)
{
  return g_object_new (GTD_TYPE_WINDOW,
                       "application", application,
                       "manager",     gtd_application_get_manager (application),
                       NULL);
}

static void
gtd_window_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtd_window_parent_class)->finalize (object);
}

static void
gtd_window_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtdWindow *self = GTD_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->priv->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_window_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtdWindow *self = GTD_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->manager = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_window_class_init (GtdWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_window_finalize;
  object_class->get_property = gtd_window_get_property;
  object_class->set_property = gtd_window_set_property;

  /**
   * GtdWindow::manager:
   *
   * A weak reference to the application's #GtdManager instance.
   */
  g_object_class_install_property (
        object_class,
        PROP_MANAGER,
        g_param_spec_object ("manager",
                             _("Manager of this window's application"),
                             _("The manager of the window's application"),
                             GTD_TYPE_MANAGER,
                             G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/window.ui");
}

static void
gtd_window_init (GtdWindow *self)
{
  self->priv = gtd_window_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gtd_window_get_manager:
 * @window: a #GtdWindow
 *
 * Retrieves a weak reference for the @window's #GtdManager.
 *
 * Returns: (transfer none): the #GtdManager of @window.
 */
GtdManager*
gtd_window_get_manager (GtdWindow *window)
{
  g_return_val_if_fail (GTD_IS_WINDOW (window), NULL);

  return window->priv->manager;
}
