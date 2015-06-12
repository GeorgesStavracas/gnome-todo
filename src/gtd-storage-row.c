/* gtd-storage-row.c
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

#include "gtd-storage.h"
#include "gtd-storage-row.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkImage        *icon;
  GtkLabel        *name;
  GtkLabel        *provider;
  GtkLabel        *enabled;
  GtkImage        *selected;

  GtdStorage      *storage;
} GtdStorageRowPrivate;

struct _GtdStorageRow
{
  GtkListBoxRow         parent;

  /*< private >*/
  GtdStorageRowPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdStorageRow, gtd_storage_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_STORAGE,
  LAST_PROP
};

static void
gtd_storage_row_finalize (GObject *object)
{
  GtdStorageRow *self = (GtdStorageRow *)object;
  GtdStorageRowPrivate *priv = gtd_storage_row_get_instance_private (self);

  g_clear_object (&priv->storage);

  G_OBJECT_CLASS (gtd_storage_row_parent_class)->finalize (object);
}

static void
gtd_storage_row_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtdStorageRow *self = GTD_STORAGE_ROW (object);

  switch (prop_id)
    {
    case PROP_STORAGE:
      g_value_set_object (value, self->priv->storage);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_storage_row_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtdStorageRow *self = GTD_STORAGE_ROW (object);

  switch (prop_id)
    {
    case PROP_STORAGE:
      self->priv->storage = g_value_get_object (value);

      if (!self->priv->storage)
        break;

      g_object_ref (self->priv->storage);

      g_object_bind_property (self->priv->storage,
                              "name",
                              self->priv->name,
                              "label",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      g_object_bind_property (self->priv->storage,
                              "provider-name",
                              self->priv->provider,
                              "label",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      g_object_bind_property (self->priv->storage,
                              "enabled",
                              self->priv->enabled,
                              "visible",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

      g_object_bind_property (self->priv->storage,
                              "icon",
                              self->priv->icon,
                              "gicon",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_storage_row_class_init (GtdStorageRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_storage_row_finalize;
  object_class->get_property = gtd_storage_row_get_property;
  object_class->set_property = gtd_storage_row_set_property;

  /**
   * GtdStorageRow::storage:
   *
   * #GtdStorage related to this row.
   */
  g_object_class_install_property (
        object_class,
        PROP_STORAGE,
        g_param_spec_object ("storage",
                             _("Storage of the row"),
                             _("The storage that this row holds"),
                             GTD_TYPE_STORAGE,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));


  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/storage-row.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageRow, icon);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageRow, name);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageRow, provider);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageRow, enabled);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageRow, selected);
}

static void
gtd_storage_row_init (GtdStorageRow *self)
{
  self->priv = gtd_storage_row_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}


/**
 * gtd_storage_row_new:
 * @storage: a #GtdStorage
 *
 * Created a new #GtdStorageRow with @account information.
 *
 * Returns: (transfer full): a new #GtdStorageRow
 */
GtkWidget*
gtd_storage_row_new (GtdStorage *storage)
{
  return g_object_new (GTD_TYPE_STORAGE_ROW,
                       "storage", storage,
                       NULL);
}

/**
 * gtd_storage_row_get_storage:
 * @row: a #GtdStorageRow
 *
 * Retrieves the #GtdStorage associated with @row.
 *
 * Returns: (transfer none): the #GtdStorage associated with @row.
 */
GtdStorage*
gtd_storage_row_get_storage (GtdStorageRow *row)
{
  g_return_val_if_fail (GTD_IS_STORAGE_ROW (row), NULL);

  return row->priv->storage;
}

/**
 * gtd_storage_row_get_selected:
 * @row: a #GtdStorageRow
 *
 * Whether @row is the currently selected row or not.
 *
 * Returns: %TRUE if the row is currently selected, %FALSE otherwise.
 */
gboolean
gtd_storage_row_get_selected (GtdStorageRow *row)
{
  g_return_val_if_fail (GTD_IS_STORAGE_ROW (row), FALSE);

  return gtk_widget_get_visible (GTK_WIDGET (row->priv->selected));
}

/**
 * gtd_storage_row_set_selected:
 * @row: a #GtdStorageRow
 * @selected: %TRUE if row is selected (i.e. the selection
 * mark is visible)
 *
 * Sets @row as the currently selected row.
 *
 * Returns:
 */
void
gtd_storage_row_set_selected (GtdStorageRow *row,
                              gboolean       selected)
{
  g_return_if_fail (GTD_IS_STORAGE_ROW (row));

  if (selected != gtk_widget_get_visible (GTK_WIDGET (row->priv->selected)))
    {
      gtk_widget_set_visible (GTK_WIDGET (row->priv->selected), selected);
    }
}
