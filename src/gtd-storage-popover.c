/* gtd-storage-popover.c
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

#include "gtd-manager.h"
#include "gtd-storage.h"
#include "gtd-storage-popover.h"
#include "gtd-storage-selector.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget            *location_provider_image;
  GtkWidget            *stack;
  GtkWidget            *storage_selector;

  GtdManager           *manager;
} GtdStoragePopoverPrivate;

struct _GtdStoragePopover
{
  GtkPopover                parent;

  /*<private>*/
  GtdStoragePopoverPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdStoragePopover, gtd_storage_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_MANAGER,
  LAST_PROP
};

static void
gtd_storage_popover__change_location_clicked (GtdStoragePopover *popover)
{
  GtdStoragePopoverPrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE_POPOVER (popover));

  priv = popover->priv;

  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "selector");
}

static void
gtd_storage_popover__storage_selected (GtdStoragePopover *popover,
                                       GtdStorage        *storage)
{
  GtdStoragePopoverPrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE_POPOVER (popover));
  g_return_if_fail (GTD_IS_STORAGE (storage));

  priv = popover->priv;

  if (storage)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (priv->location_provider_image),
                                gtd_storage_get_icon (storage),
                                GTK_ICON_SIZE_BUTTON);
      //gtk_widget_set_tooltip_text ()
    }
}

static void
gtd_storage_popover_finalize (GObject *object)
{
  GtdStoragePopover *self = (GtdStoragePopover *)object;
  GtdStoragePopoverPrivate *priv = gtd_storage_popover_get_instance_private (self);

  G_OBJECT_CLASS (gtd_storage_popover_parent_class)->finalize (object);
}

static void
gtd_storage_popover_constructed (GObject *object)
{
  GtdStoragePopoverPrivate *priv;
  GtdStorage *storage;

  G_OBJECT_CLASS (gtd_storage_popover_parent_class)->constructed (object);

  priv = GTD_STORAGE_POPOVER (object)->priv;
  storage = gtd_storage_selector_get_selected_storage (GTD_STORAGE_SELECTOR (priv->storage_selector));

  g_object_bind_property (object,
                          "manager",
                          priv->storage_selector,
                          "manager",
                          G_BINDING_DEFAULT);

  if (storage)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (priv->location_provider_image),
                                gtd_storage_get_icon (storage),
                                GTK_ICON_SIZE_BUTTON);
    }
}

static void
gtd_storage_popover_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtdStoragePopover *self = GTD_STORAGE_POPOVER (object);

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
gtd_storage_popover_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtdStoragePopover *self = GTD_STORAGE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      if (self->priv->manager != g_value_get_object (value))
        {
          self->priv->manager = g_value_get_object (value);
          g_object_notify (object, "manager");
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_storage_popover_class_init (GtdStoragePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_storage_popover_finalize;
  object_class->constructed = gtd_storage_popover_constructed;
  object_class->get_property = gtd_storage_popover_get_property;
  object_class->set_property = gtd_storage_popover_set_property;

  /**
   * GtdStoragePopover::manager:
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
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/storage-popover.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdStoragePopover, location_provider_image);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStoragePopover, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStoragePopover, storage_selector);

  gtk_widget_class_bind_template_callback (widget_class, gtd_storage_popover__change_location_clicked);
  gtk_widget_class_bind_template_callback (widget_class, gtd_storage_popover__storage_selected);
}

static void
gtd_storage_popover_init (GtdStoragePopover *self)
{
  self->priv = gtd_storage_popover_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gtd_storage_popover_new (void)
{
  return g_object_new (GTD_TYPE_STORAGE_POPOVER, NULL);
}
