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

#include "gtd-application.h"
#include "gtd-manager.h"
#include "gtd-storage.h"
#include "gtd-storage-row.h"
#include "gtd-storage-selector.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget                 *listbox;
  GtkWidget                 *local_check;

  /* stub rows */
  GtkWidget                 *exchange_stub_row;
  GtkWidget                 *google_stub_row;
  GtkWidget                 *owncloud_stub_row;
  GtkWidget                 *local_row;

  GtdManager                *manager;

  gint                      select_default : 1;
  gint                      show_local_storage : 1;
} GtdStorageSelectorPrivate;

struct _GtdStorageSelector
{
  GtkBox                     parent;

  /*< private >*/
  GtdStorageSelectorPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdStorageSelector, gtd_storage_selector, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_MANAGER,
  PROP_SELECT_DEFAULT,
  PROP_SHOW_LOCAL,
  LAST_PROP
};

enum {
  STORAGE_SELECTED,
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
      GtdStorage *storage;
      GList *children;
      GList *l;

      children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));
      storage = gtd_storage_row_get_storage (GTD_STORAGE_ROW (row));

      for (l = children; l != NULL; l = l->next)
        {
          if (GTD_IS_STORAGE_ROW (l->data))
            gtd_storage_row_set_selected (l->data, FALSE);
        }

      /*
       * If the account has it's calendars disabled, we cannot let it
       * be a default storage location. Instead, open the Control Center
       * to give the user the ability to change it.
       */
      if (gtd_storage_get_enabled (storage))
        {
          gtd_storage_row_set_selected (GTD_STORAGE_ROW (row), TRUE);
          g_signal_emit (selector, signals[STORAGE_SELECTED], 0, storage);
        }
      else
        {
          spawn ((gchar*) gtd_storage_get_id (storage), NULL);
        }

      g_list_free (children);
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
      GtdStorage *local_storage;
      GList *children;
      GList *l;

      children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));
      local_storage = gtd_storage_row_get_storage (GTD_STORAGE_ROW (priv->local_row));

      for (l = children; l != NULL; l = l->next)
        {
          if (GTD_IS_STORAGE_ROW (l->data))
            gtd_storage_row_set_selected (l->data, FALSE);
        }

      g_list_free (children);

      /*
       * Sets the storage location to "local", and don't unset it if the
       * check gets deactivated.
       */
      g_signal_emit (selector, signals[STORAGE_SELECTED], 0, local_storage);
    }
  else
    {
      g_signal_emit (selector, signals[STORAGE_SELECTED], 0, NULL);
    }
}

static void
gtd_storage_selector__remove_storage (GtdStorageSelector *selector,
                                      GtdStorage         *storage)
{
  GtdStorageSelectorPrivate *priv;
  GtkWidget *row;
  GList *children;
  GList *l;
  gint exchange;
  gint google;
  gint owncloud;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));
  g_return_if_fail (GTD_IS_STORAGE (storage));

  priv = selector->priv;
  children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));
  exchange = google = owncloud = 0;

  for (l = children; l != NULL; l = l->next)
    {
      GtdStorage *row_storage;
      const gchar *provider;

      if (!GTD_IS_STORAGE_ROW (l->data))
        continue;

      row_storage = gtd_storage_row_get_storage (l->data);
      provider = gtd_storage_get_provider (row_storage);

      if (row_storage == storage)
        {
          gtk_widget_destroy (l->data);
        }
      else
        {
          if (g_strcmp0 (provider, "exchange") == 0)
            exchange++;
          else if (g_strcmp0 (provider, "google") == 0)
            google++;
          else if (g_strcmp0 (provider, "owncloud") == 0)
            owncloud++;
        }
    }

  gtk_widget_set_visible (priv->exchange_stub_row, exchange == 0);
  gtk_widget_set_visible (priv->google_stub_row, google == 0);
  gtk_widget_set_visible (priv->owncloud_stub_row, owncloud == 0);

  g_list_free (children);
}

static void
gtd_storage_selector__add_storage (GtdStorageSelector *selector,
                                   GtdStorage         *storage)
{
  GtdStorageSelectorPrivate *priv;
  GtkWidget *row;
  const gchar *provider;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));
  g_return_if_fail (GTD_IS_STORAGE (storage));

  priv = selector->priv;

  row = gtd_storage_row_new (storage);
  provider = gtd_storage_get_provider (storage);

  gtk_container_add (GTK_CONTAINER (priv->listbox), row);

  /* track the local provider row */
  if (g_strcmp0 (provider, "local") == 0)
    {
      gtk_widget_set_visible (row, priv->show_local_storage);
      priv->local_row = row;
    }

  /* Auto selects the default storage row when needed */
  if (priv->select_default &&
      gtd_storage_get_is_default (storage) &&
      !gtd_storage_selector_get_selected_storage (selector))
    {
      gtd_storage_selector_set_selected_storage (selector, storage);
    }

  /* hide the related stub row */
  if (g_strcmp0 (provider, "exchange") == 0)
    gtk_widget_hide (priv->exchange_stub_row);
  else if (g_strcmp0 (provider, "google") == 0)
    gtk_widget_hide (priv->google_stub_row);
  else if (g_strcmp0 (provider, "owncloud") == 0)
    gtk_widget_hide (priv->owncloud_stub_row);
}

static void
gtd_storage_selector__fill_accounts (GtdStorageSelector *selector)
{
  GtdStorageSelectorPrivate *priv;
  GList *storage_locations;
  GList *l;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));

  priv = selector->priv;

  /* load accounts */
  storage_locations = gtd_manager_get_storage_locations (priv->manager);

  for (l = storage_locations; l != NULL; l = l->next)
    gtd_storage_selector__add_storage (selector, l->data);

  g_list_free (storage_locations);
}

static gint
sort_func (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       user_data)
{
  GtdStorageRow *r1;
  GtdStorageRow *r2;
  GtdStorage *storage1;
  GtdStorage *storage2;
  gint retval;

  if (!GTD_IS_STORAGE_ROW (row1))
    return 1;
  else if (!GTD_IS_STORAGE_ROW (row2))
    return -1;

  storage1 = gtd_storage_row_get_storage (GTD_STORAGE_ROW (row1));
  storage2 = gtd_storage_row_get_storage (GTD_STORAGE_ROW (row2));

  return gtd_storage_compare (storage1, storage2);
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

    case PROP_SELECT_DEFAULT:
      g_value_set_boolean (value, self->priv->select_default);
      break;

    case PROP_SHOW_LOCAL:
      g_value_set_boolean (value, self->priv->show_local_storage);
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

      gtd_storage_selector__fill_accounts (self);

      g_signal_connect_swapped (self->priv->manager,
                                "storage-added",
                                G_CALLBACK (gtd_storage_selector__add_storage),
                                object);

      g_signal_connect_swapped (self->priv->manager,
                                "storage-removed",
                                G_CALLBACK (gtd_storage_selector__remove_storage),
                                object);

      g_object_notify (object, "manager");
      break;

    case PROP_SELECT_DEFAULT:
      gtd_storage_selector_set_select_default (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LOCAL:
      gtd_storage_selector_show_local (self, g_value_get_boolean (value));
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
  signals[STORAGE_SELECTED] = g_signal_new ("storage-selected",
                                             GTD_TYPE_STORAGE_SELECTOR,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL,
                                             NULL,
                                             NULL,
                                             G_TYPE_NONE,
                                             1,
                                             GTD_TYPE_STORAGE);

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

  /**
   * GtdStorageSelector::show-local-storage:
   *
   * Whether it should show a row for the local storage.
   */
  g_object_class_install_property (
        object_class,
        PROP_SHOW_LOCAL,
        g_param_spec_boolean ("show-local",
                              _("Show local storage row"),
                              _("Whether should show a local storage row instead of a checkbox"),
                              FALSE,
                              G_PARAM_READWRITE));

  /**
   * GtdStorageSelector::select-default:
   *
   * Whether it should auto selects the default storage location row.
   */
  g_object_class_install_property (
        object_class,
        PROP_SELECT_DEFAULT,
        g_param_spec_boolean ("select-default",
                              _("Selects default storage row"),
                              _("Whether should select the default storage row"),
                              FALSE,
                              G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/storage-selector.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, exchange_stub_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, google_stub_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtdStorageSelector, local_check);
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

/**
 * gtd_storage_selector_show_local:
 *
 * Shows a row for local storage item.
 *
 * Returns:
 */
void
gtd_storage_selector_show_local (GtdStorageSelector *selector,
                                 gboolean            show)
{
  GtdStorageSelectorPrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));

  priv = selector->priv;

  if (priv->show_local_storage != show)
    {
      priv->show_local_storage = show;

      gtk_widget_set_visible (priv->local_check, !show);
      gtk_widget_set_visible (priv->local_row, show);

      g_object_notify (G_OBJECT (selector), "show-local");
    }
}

/**
 * gtd_storage_selector_get_select_default:
 * @selector: a #GtdStorageSelector
 *
 * Whether the default storage location is selected by default.
 *
 * Returns: %TRUE if the default storage location is selected automatically,
 * %FALSE otherwise.
 */
gboolean
gtd_storage_selector_get_select_default (GtdStorageSelector *selector)
{
  g_return_val_if_fail (GTD_IS_STORAGE_SELECTOR (selector), FALSE);

  return selector->priv->select_default;
}

/**
 * gtd_storage_selector_set_select_default:
 * @selector: a #GtdStorageSelector
 * @select_default: %TRUE to auto select the default storage location.
 *
 * Whether @selector should select the default storage location by default.
 *
 * Returns:
 */
void
gtd_storage_selector_set_select_default (GtdStorageSelector *selector,
                                         gboolean            select_default)
{
  GtdStorageSelectorPrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));

  priv = selector->priv;

  if (priv->select_default != select_default)
    {
      priv->select_default = select_default;

      if (select_default)
        {
          GList *children;
          GList *l;

          /* Select the appropriate row */
          children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

          for (l = children; l != NULL; l = l->next)
            {
              if (GTD_IS_STORAGE_ROW (l->data))
                {
                  GtdStorage *storage = gtd_storage_row_get_storage (l->data);

                  if (gtd_storage_get_is_default (storage))
                    {
                      gtd_storage_row_set_selected (l->data, TRUE);
                      g_signal_emit (selector, signals[STORAGE_SELECTED], 0, storage);
                    }
                }
            }

          g_list_free (children);
        }

      g_object_notify (G_OBJECT (selector), "select-default");
    }
}

/**
 * gtd_storage_selector_get_selected_storage:
 * @selector: a #GtdStorageSelector
 *
 * Retrieves the currently selected #GtdStorage, or %NULL if
 * none is selected.
 *
 * Returns: (transfer none): the selected #GtdStorage
 */
GtdStorage*
gtd_storage_selector_get_selected_storage (GtdStorageSelector *selector)
{
  GtdStorageSelectorPrivate *priv;
  GtdStorage *storage;
  GList *children;
  GList *l;

  g_return_val_if_fail (GTD_IS_STORAGE_SELECTOR (selector), NULL);

  priv = selector->priv;
  storage = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

  for (l = children; l != NULL; l = l->next)
    {
      if (GTD_IS_STORAGE_ROW (l->data) && gtd_storage_row_get_selected (l->data))
        {
          storage = gtd_storage_row_get_storage (l->data);
          break;
        }
    }

  g_list_free (children);

  return storage;
}

/**
 * gtd_storage_selector_set_selected_storage:
 * @selector: a #GtdStorageSelector
 * @storage: a #GtdStorage
 *
 * Selects @storage in the given #GtdStorageSelector.
 *
 * Returns:
 */
void
gtd_storage_selector_set_selected_storage (GtdStorageSelector *selector,
                                           GtdStorage         *storage)
{
  GtdStorageSelectorPrivate *priv;
  GList *children;
  GList *l;

  g_return_if_fail (GTD_IS_STORAGE_SELECTOR (selector));

  priv = selector->priv;
  children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

  for (l = children; l != NULL; l = l->next)
    {
      if (GTD_IS_STORAGE_ROW (l->data))
        {
          gtd_storage_row_set_selected (l->data, gtd_storage_row_get_storage (l->data) == storage);
          g_signal_emit (selector, signals[STORAGE_SELECTED], 0, storage);
          break;
        }
    }

  g_list_free (children);
}
