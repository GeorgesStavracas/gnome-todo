/* gtd-initial-setup-window.c
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
#include "gtd-goa-row.h"
#include "gtd-initial-setup-window.h"
#include "gtd-manager.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkWidget                 *listbox;

  /* stub rows */
  GtkWidget                 *exchange_stub_row;
  GtkWidget                 *google_stub_row;
  GtkWidget                 *owncloud_stub_row;

  GtdManager                *manager;
} GtdInitialSetupWindowPrivate;

struct _GtdInitialSetupWindow
{
  GtkApplicationWindow          parent;

  /*<private>*/
  GtdInitialSetupWindowPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdInitialSetupWindow, gtd_initial_setup_window, GTK_TYPE_APPLICATION_WINDOW)

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
gtd_initial_setup_window__listbox_row_activated (GtdInitialSetupWindow *window,
                                                 GtkWidget             *row)
{
  GtdInitialSetupWindowPrivate *priv;

  g_return_if_fail (GTD_IS_INITIAL_SETUP_WINDOW (window));

  priv = window->priv;

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
        }
    }
}

static void
gtd_initial_setup_window__fill_accounts (GtdInitialSetupWindow *window)
{
  GtdInitialSetupWindowPrivate *priv;
  GoaClient *goa_client;
  GList *accounts;
  GList *l;

  g_return_if_fail (GTD_IS_INITIAL_SETUP_WINDOW (window));

  priv = GTD_INITIAL_SETUP_WINDOW (window)->priv;
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


static void
gtd_initial_setup_window_constructed (GObject *object)
{
  GtdInitialSetupWindowPrivate *priv;

  G_OBJECT_CLASS (gtd_initial_setup_window_parent_class)->constructed (object);

  priv = GTD_INITIAL_SETUP_WINDOW (object)->priv;

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
gtd_initial_setup_window_finalize (GObject *object)
{
  GtdInitialSetupWindow *self = (GtdInitialSetupWindow *)object;
  GtdInitialSetupWindowPrivate *priv = gtd_initial_setup_window_get_instance_private (self);

  G_OBJECT_CLASS (gtd_initial_setup_window_parent_class)->finalize (object);
}

static void
gtd_initial_setup_window_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtdInitialSetupWindow *self = GTD_INITIAL_SETUP_WINDOW (object);

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
gtd_initial_setup_window_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtdInitialSetupWindow *self = GTD_INITIAL_SETUP_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->manager = g_value_get_object (value);

      if (gtd_manager_is_goa_client_ready (self->priv->manager))
        {
          gtd_initial_setup_window__fill_accounts (self);
        }
      else
        {
          g_signal_connect_swapped (self->priv->manager,
                                    "notify::goa-client-ready",
                                    G_CALLBACK (gtd_initial_setup_window__fill_accounts),
                                    self);
        }

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_initial_setup_window_class_init (GtdInitialSetupWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_initial_setup_window_finalize;
  object_class->constructed = gtd_initial_setup_window_constructed;
  object_class->get_property = gtd_initial_setup_window_get_property;
  object_class->set_property = gtd_initial_setup_window_set_property;

  /**
   * GtdInitialSetupWindow::manager:
   *
   * Manager of the application.
   */
  g_object_class_install_property (
        object_class,
        PROP_MANAGER,
        g_param_spec_object ("manager",
                             _("Manager of the task"),
                             _("The singleton manager instance of the task"),
                             GTD_TYPE_MANAGER,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/initial-setup.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdInitialSetupWindow, exchange_stub_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtdInitialSetupWindow, google_stub_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtdInitialSetupWindow, listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtdInitialSetupWindow, owncloud_stub_row);

  gtk_widget_class_bind_template_callback (widget_class, gtd_initial_setup_window__listbox_row_activated);
}

static void
gtd_initial_setup_window_init (GtdInitialSetupWindow *self)
{
  self->priv = gtd_initial_setup_window_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gtd_initial_setup_window_new (GtdApplication *application)
{
  g_return_val_if_fail (GTD_IS_APPLICATION (application), NULL);

  return g_object_new (GTD_TYPE_INITIAL_SETUP_WINDOW,
                       "application", application,
                       "manager", gtd_application_get_manager (application),
                       NULL);
}
