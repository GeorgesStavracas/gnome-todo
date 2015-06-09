/* gtd-goa-row.c
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

#include "gtd-goa-row.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkImage        *icon;
  GtkLabel        *name;
  GtkLabel        *provider;
  GtkLabel        *enabled;
  GtkImage        *selected;

  GoaAccount      *account;
} GtdGoaRowPrivate;

struct _GtdGoaRow
{
  GtkListBoxRow     parent;

  /*< private >*/
  GtdGoaRowPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdGoaRow, gtd_goa_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_ACCOUNT,
  LAST_PROP
};

static gchar*
get_account_icon_name (GoaAccount *account)
{
  return g_strdup_printf ("goa-account-%s", goa_account_get_provider_type (account));
}

static void
gtd_goa_row_finalize (GObject *object)
{
  GtdGoaRow *self = (GtdGoaRow *)object;
  GtdGoaRowPrivate *priv = gtd_goa_row_get_instance_private (self);

  G_OBJECT_CLASS (gtd_goa_row_parent_class)->finalize (object);
}

static void
gtd_goa_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtdGoaRow *self = GTD_GOA_ROW (object);

  switch (prop_id)
    {
    case PROP_ACCOUNT:
      g_value_set_object (value, self->priv->account);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_goa_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtdGoaRow *self = GTD_GOA_ROW (object);
  gchar *str = NULL;

  switch (prop_id)
    {
    case PROP_ACCOUNT:
      self->priv->account = g_value_get_object (value);
      str = get_account_icon_name (self->priv->account);

      gtk_label_set_label (self->priv->provider, goa_account_get_provider_name (self->priv->account));
      gtk_label_set_label (self->priv->name, goa_account_get_identity (self->priv->account));
      g_object_bind_property (self->priv->account,
                              "calendar-disabled",
                              self->priv->enabled,
                              "visible",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      gtk_image_set_from_icon_name (self->priv->icon,
                                    str,
                                    GTK_ICON_SIZE_DND);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

  g_free (str);
}

static void
gtd_goa_row_class_init (GtdGoaRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_goa_row_finalize;
  object_class->get_property = gtd_goa_row_get_property;
  object_class->set_property = gtd_goa_row_set_property;

  /**
   * GtdGoaRow::account:
   *
   * Account related to this row.
   */
  g_object_class_install_property (
        object_class,
        PROP_ACCOUNT,
        g_param_spec_object ("account",
                             _("Account of the row"),
                             _("The account that this row holds"),
                             GOA_TYPE_ACCOUNT,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/goa-row.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdGoaRow, icon);
  gtk_widget_class_bind_template_child_private (widget_class, GtdGoaRow, name);
  gtk_widget_class_bind_template_child_private (widget_class, GtdGoaRow, provider);
  gtk_widget_class_bind_template_child_private (widget_class, GtdGoaRow, enabled);
  gtk_widget_class_bind_template_child_private (widget_class, GtdGoaRow, selected);
}

static void
gtd_goa_row_init (GtdGoaRow *self)
{
  self->priv = gtd_goa_row_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}


/**
 * gtd_goa_row_new:
 * @account: a #GoaAccount
 *
 * Created a new #GtdGoaRow with @account information.
 *
 * Returns: (transfer full): a new #GtdGoaRow
 */
GtkWidget*
gtd_goa_row_new (GoaAccount *account)
{
  return g_object_new (GTD_TYPE_GOA_ROW,
                       "account", account,
                       NULL);
}

/**
 * gtd_goa_row_get_account:
 * @row: a #GtdGoaRow
 *
 * Retrieves the #GoaAccount associated with @row.
 *
 * Returns: (transfer none): the #GoaAccount associated with @row.
 */
GoaAccount*
gtd_goa_row_get_account (GtdGoaRow *row)
{
  g_return_val_if_fail (GTD_IS_GOA_ROW (row), NULL);

  return row->priv->account;
}

/**
 * gtd_goa_row_get_selected:
 * @row: a #GtdGoaRow
 *
 * Whether @row is the currently selected row or not.
 *
 * Returns: %TRUE if the row is currently selected, %FALSE otherwise.
 */
gboolean
gtd_goa_row_get_selected (GtdGoaRow *row)
{
  g_return_val_if_fail (GTD_IS_GOA_ROW (row), FALSE);

  return gtk_widget_get_visible (GTK_WIDGET (row->priv->selected));
}

/**
 * gtd_goa_row_set_selected:
 * @row: a #GtdGoaRow
 * @selected: %TRUE if row is selected (i.e. the selection
 * mark is visible)
 *
 * Sets @row as the currently selected row.
 *
 * Returns:
 */
void
gtd_goa_row_set_selected (GtdGoaRow *row,
                          gboolean   selected)
{
  g_return_if_fail (GTD_IS_GOA_ROW (row));

  if (selected != gtk_widget_get_visible (GTK_WIDGET (row->priv->selected)))
    {
      gtk_widget_set_visible (GTK_WIDGET (row->priv->selected), selected);
    }
}
