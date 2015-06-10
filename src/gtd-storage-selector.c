/* gtd-storage-selector.c
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
#include "gtd-manager.h"
#include "gtd-storage-selector.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget                 *listbox;

  /* stub rows */
  GtkWidget                 *exchange_stub_row;
  GtkWidget                 *google_stub_row;
  GtkWidget                 *owncloud_stub_row;

  GtdManager                *manager;
} GtdStorageSelectorPrivate;

struct _GtdStorageSelector
{
  GtkBox                     parent;

  /*< private >*/
  GtdStorageSelectorPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdStorageSelector, gtd_storage_selector, GTK_TYPE_BOX)

const gchar *supported_providers[] = {
  "exchange",
  "google",
  "owncloud",
  NULL
};

enum {
  PROP_0,
  PROP_MANAGER,
  LAST_PROP
};

enum {
  LOCATION_SELECTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void
spawn (gchar *action,
       gchar *arg)
{
  gchar *command[] = {"gnome-control-center", "online-accounts", action, arg, NULL};
  g_spawn_async (NULL, command, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL, NULL, NULL, NULL, NULL);
}

/**
 * display_header_func:
 *
 * Shows a separator before each row.
 */
static void
display_header_func (GtkListBoxRow *row,
                     GtkListBoxRow *before,
                     gpointer       user_data)
{
  if (before != NULL)
    {
      GtkWidget *header;

      header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

      gtk_list_box_row_set_header (row, header);
      gtk_widget_show (header);
    }
}

static void
gtd_storage_selector__listbox_row_activated (GtdStorageSelector *selector,
                                             GtkWidget          *row)
{
  GtdStorageSelectorPrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));

  priv = selector->priv;

  /* The row is either one of the stub rows, or a GtdGoaRow */
  if (row == priv->google_stub_row)
    {
      spawn ("add", "google");
    }
  else if (row == priv->owncloud_stub_row)
    {
      spawn ("add", "owncloud");
    }
  else if (row == priv->exchange_stub_row)
    {
      spawn ("add", "exchange");
    }
  else
    {
      GoaAccount *account;
      GList *children;
      GList *l;

      children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));
      account = gtd_goa_row_get_account (GTD_GOA_ROW (row));

      for (l = children; l != NULL; l = l->next)
        {
          if (GTD_IS_GOA_ROW (l->data))
            gtd_goa_row_set_selected (l->data, FALSE);
        }

      g_list_free (children);

      /*
       * If the account has it's calendars disabled, we cannot let it
       * be a default storage location. Instead, open the Control Center
       * to give the user the ability to change it.
       */
      if (goa_account_get_calendar_disabled (account))
        {
          spawn ((gchar*) goa_account_get_id (account), NULL);
        }
      else
        {
          gtd_goa_row_set_selected (GTD_GOA_ROW (row), TRUE);
          g_signal_emit (selector, signals[LOCATION_SELECTED], 0, goa_account_get_id (account));
        }
    }
}

static void
gtd_storage_selector__check_toggled (GtdStorageSelector *selector,
                                     GtkToggleButton    *check)
{
  GtdStorageSelectorPrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));

  priv = selector->priv;

  /*
   * Unset the currently selected storage location row when the check button is
   * activated. No need to do this when deactivated, since we already did.
   */

  if (gtk_toggle_button_get_active (check))
    {
      GList *children;
      GList *l;

      children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

      for (l = children; l != NULL; l = l->next)
        {
          if (GTD_IS_GOA_ROW (l->data))
            gtd_goa_row_set_selected (l->data, FALSE);
        }

      g_list_free (children);

      /*
       * Sets the storage location to "local", and don't unset it if the
       * check gets deactivated.
       */
      g_signal_emit (selector, signals[LOCATION_SELECTED], 0, "local");
    }
  else
    {
      g_signal_emit (selector, signals[LOCATION_SELECTED], 0, NULL);
    }
}

static void
gtd_storage_selector__fill_accounts (GtdStorageSelector *selector)
{
  GtdStorageSelectorPrivate *priv;
  GoaClient *goa_client;
  GList *accounts;
  GList *l;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));

  priv = GTD_STORAGE_SELECTOR (selector)->priv;
  goa_client = gtd_manager_get_goa_client (priv->manager);

  /* load accounts */
  accounts = goa_client_get_accounts (goa_client);

  for (l = accounts; l != NULL; l = l->next)
    {
      const gchar* provider;
      GoaObject *object;
      GoaAccount *account;

      object = l->data;
      account = goa_object_get_account (object);
      provider = goa_account_get_provider_type (account);

      if (g_strv_contains (supported_providers, provider))
        {
          GtkWidget *row;

          row = gtd_goa_row_new (account);

          gtk_container_add (GTK_CONTAINER (priv->listbox), row);

          /* hide the related stub row */
          if (g_strcmp0 (provider, "exchange") == 0)
            gtk_widget_hide (priv->exchange_stub_row);
          if (g_strcmp0 (provider, "google") == 0)
            gtk_widget_hide (priv->google_stub_row);
          if (g_strcmp0 (provider, "owncloud") == 0)
            gtk_widget_hide (priv->owncloud_stub_row);
        }
    }

  g_list_free_full (accounts, g_object_unref);
}

static gint
sort_func (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       user_data)
{
  GoaAccount *a1;
  GoaAccount *a2;
  GtdGoaRow *r1;
  GtdGoaRow *r2;
  gint retval;

  if (!GTD_IS_GOA_ROW (row1))
    return 1;
  if (!GTD_IS_GOA_ROW (row2))
    return -1;

  r1 = GTD_GOA_ROW (row1);
  r2 = GTD_GOA_ROW (row2);

  a1 = gtd_goa_row_get_account (r1);
  a2 = gtd_goa_row_get_account (r2);

  retval = g_strcmp0 (goa_account_get_provider_type (a1), goa_account_get_provider_type (a2));

  if (retval != 0)
    return retval;
  else
    return g_strcmp0 (goa_account_get_identity (a1), goa_account_get_identity (a2));
}

static void
gtd_storage_selector_constructed (GObject *object)
{
  GtdStorageSelectorPrivate *priv;

  G_OBJECT_CLASS (gtd_storage_selector_parent_class)->constructed (object);

  priv = GTD_STORAGE_SELECTOR (object)->priv;

  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->listbox),
                                display_header_func,
                                NULL,
                                NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->listbox),
                              (GtkListBoxSortFunc) sort_func,
                              NULL,
                              NULL);
}

static void
gtd_storage_selector_finalize (GObject *object)
{
  GtdStorageSelector *self = (GtdStorageSelector *)object;
  GtdStorageSelectorPrivate *priv = gtd_storage_selector_get_instance_private (self);

  G_OBJECT_CLASS (gtd_storage_selector_parent_class)->finalize (object);
}

static void
gtd_storage_selector_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtdStorageSelector *self = GTD_STORAGE_SELECTOR (object);

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
gtd_storage_selector_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtdStorageSelector *self = GTD_STORAGE_SELECTOR (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->manager = g_value_get_object (value);

      if (!self->priv->manager)
        break;

      if (gtd_manager_is_goa_client_ready (self->priv->manager))
        {
          gtd_storage_selector__fill_accounts (self);
        }
      else
        {
          g_signal_connect_swapped (self->priv->manager,
                                    "notify::goa-client-ready",
                                    G_CALLBACK (gtd_storage_selector__fill_accounts),
                                    self);
        }

      g_object_notify (object, "manager");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_storage_selector_class_init (GtdStorageSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_storage_selector_finalize;
  object_class->constructed = gtd_storage_selector_constructed;
  object_class->get_property = gtd_storage_selector_get_property;
  object_class->set_property = gtd_storage_selector_set_property;

  /**
   * GtdStorageSelector::location-selected:
   *
   * Emitted when a storage location is selected.
   */
  signals[LOCATION_SELECTED] = g_signal_new ("location-selected",
                                             GTD_TYPE_STORAGE_SELECTOR,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL,
                                             NULL,
                                             NULL,
                                             G_TYPE_NONE,
                                             1,
                                             G_TYPE_STRING);

  /**
   * GtdStorageSelector::manager:
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
                             G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/storage-selector.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, exchange_stub_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, google_stub_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, owncloud_stub_row);

  gtk_widget_class_bind_template_callback (widget_class, gtd_storage_selector__check_toggled);
  gtk_widget_class_bind_template_callback (widget_class, gtd_storage_selector__listbox_row_activated);
}

static void
gtd_storage_selector_init (GtdStorageSelector *self)
{
  self->priv = gtd_storage_selector_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gtd_storage_selector_new:
 *
 * Creates a new #GtdStorageSelector.
 *
 * Returns: (transfer full): a new #GtdStorageSelector
 */
GtkWidget*
gtd_storage_selector_new (void)
{
  return g_object_new (GTD_TYPE_STORAGE_SELECTOR, NULL);
}
