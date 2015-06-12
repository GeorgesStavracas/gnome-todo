/* gtd-manager.c
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
#include "gtd-task.h"
#include "gtd-task-list.h"

#include <glib/gi18n.h>
#include <libecal/libecal.h>
#include <libedataserverui/libedataserverui.h>

typedef struct
{
  GHashTable            *clients;
  GList                 *task_lists;
  ECredentialsPrompter  *credentials_prompter;
  ESourceRegistry       *source_registry;

  /* Online accounts */
  GoaClient             *goa_client;
  gboolean               goa_client_ready;
  GList                 *storage_locations;

  GSettings             *settings;

  /*
   * Small flag that contains the number of sources
   * that still have to be loaded. When this number
   * reaches 0, all sources are loaded.
   */
  gint                   load_sources;
} GtdManagerPrivate;

struct _GtdManager
{
   GtdObject           parent;

  /*< private >*/
  GtdManagerPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdManager, gtd_manager, GTD_TYPE_OBJECT)

const gchar *supported_providers[] = {
  "exchange",
  "google",
  "owncloud",
  NULL
};

enum
{
  LIST_ADDED,
  LIST_CHANGED,
  LIST_REMOVED,
  STORAGE_ADDED,
  STORAGE_CHANGED,
  STORAGE_REMOVED,
  NUM_SIGNALS
};

enum
{
  PROP_0,
  PROP_GOA_CLIENT,
  PROP_GOA_CLIENT_READY,
  PROP_SOURCE_REGISTRY,
  LAST_PROP
};

static guint signals[NUM_SIGNALS] = { 0, };

static void
gtd_manager__goa_account_removed_cb (GoaClient  *client,
                                     GoaObject  *object,
                                     GtdManager *manager)
{
  GtdManagerPrivate *priv;
  GoaAccount *account;

  g_return_if_fail (GTD_IS_MANAGER (manager));

  priv = manager->priv;
  account = goa_object_get_account (object);

  if (g_strv_contains (supported_providers, goa_account_get_provider_type (account)))
    {
      GList *l;

      for (l = priv->storage_locations; l != NULL; l = l->next)
        {
          if (g_strcmp0 (goa_account_get_id (account), gtd_storage_get_id (l->data)) == 0)
            {
              GtdStorage *storage = l->data;

              if (gtd_storage_get_is_default (storage))
                {
                  g_settings_set_string (priv->settings,
                                         "storage-location",
                                         "local");
                }

              priv->storage_locations = g_list_remove (priv->storage_locations, storage);

              g_object_unref (storage);

              g_signal_emit (manager, signals[STORAGE_REMOVED], 0, storage);
              break;
            }
        }
    }
}

static void
gtd_manager__goa_account_changed_cb (GoaClient  *client,
                                     GoaObject  *object,
                                     GtdManager *manager)
{
  GtdManagerPrivate *priv;
  GoaAccount *account;

  g_return_if_fail (GTD_IS_MANAGER (manager));

  priv = manager->priv;
  account = goa_object_get_account (object);

  if (g_strv_contains (supported_providers, goa_account_get_provider_type (account)))
    {
      GList *l;

      for (l = priv->storage_locations; l != NULL; l = l->next)
        {
          if (g_strcmp0 (goa_account_get_id (account), gtd_storage_get_id (l->data)) == 0)
            {
              g_signal_emit (manager, signals[STORAGE_CHANGED], 0, l->data);
              break;
            }
        }
    }
}

static void
gtd_manager__goa_account_added_cb (GoaClient  *client,
                                   GoaObject  *object,
                                   GtdManager *manager)
{
  GtdManagerPrivate *priv;
  GoaAccount *account;

  g_return_if_fail (GTD_IS_MANAGER (manager));

  priv = manager->priv;
  account = goa_object_get_account (object);

  if (g_strv_contains (supported_providers, goa_account_get_provider_type (account)))
    {
      GtdStorage *storage;
      gchar *default_location;

      default_location = g_settings_get_string (priv->settings, "storage-location");

      storage = gtd_storage_new (goa_account_get_id (account),
                                 goa_account_get_provider_type (account),
                                 goa_account_get_provider_name (account),
                                 goa_account_get_identity (account));

      gtd_storage_set_enabled (storage, !goa_account_get_calendar_disabled (account));
      gtd_storage_set_is_default (storage, g_strcmp0 (gtd_storage_get_id (storage), default_location) == 0);

      priv->storage_locations = g_list_insert_sorted (priv->storage_locations,
                                                      storage,
                                                      (GCompareFunc) gtd_storage_compare);

      g_signal_emit (manager, signals[STORAGE_ADDED], 0, storage);

      g_free (default_location);
    }
}

static void
gtd_manager__goa_client_finish_cb (GObject      *client,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GtdManagerPrivate *priv;
  GError *error;

  g_return_if_fail (GTD_IS_MANAGER (user_data));

  priv = GTD_MANAGER (user_data)->priv;
  error = NULL;

  priv->goa_client_ready = TRUE;
  priv->goa_client = goa_client_new_finish (result, &error);

  if (!error)
    {
      GList *accounts;
      GList *l;
      gchar *default_location;

      /* Load each supported GoaAccount into a GtdStorage */
      accounts = goa_client_get_accounts (priv->goa_client);
      default_location = g_settings_get_string (priv->settings, "storage-location");

      for (l = accounts; l != NULL; l = l->next)
        {
          GoaObject *object;
          GoaAccount *account;

          object = l->data;
          account = goa_object_get_account (object);

          if (g_strv_contains (supported_providers, goa_account_get_provider_type (account)))
            {
              GtdStorage *storage;

              storage = gtd_storage_new (goa_account_get_id (account),
                                         goa_account_get_provider_type (account),
                                         goa_account_get_provider_name (account),
                                         goa_account_get_identity (account));

              gtd_storage_set_enabled (storage, !goa_account_get_calendar_disabled (account));
              gtd_storage_set_is_default (storage, g_strcmp0 (gtd_storage_get_id (storage), default_location) == 0);

              priv->storage_locations = g_list_insert_sorted (priv->storage_locations,
                                                              storage,
                                                              (GCompareFunc) gtd_storage_compare);

              g_signal_emit (user_data, signals[STORAGE_ADDED], 0, storage);
            }
        }

      /* Connect GoaClient signals */
      g_signal_connect (priv->goa_client,
                        "account-added",
                        G_CALLBACK (gtd_manager__goa_account_added_cb),
                        user_data);

      g_signal_connect (priv->goa_client,
                        "account-changed",
                        G_CALLBACK (gtd_manager__goa_account_changed_cb),
                        user_data);

      g_signal_connect (priv->goa_client,
                        "account-removed",
                        G_CALLBACK (gtd_manager__goa_account_removed_cb),
                        user_data);

      g_list_free_full (accounts, g_object_unref);
      g_free (default_location);
    }
  else
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error loading GNOME Online Accounts"),
                 error->message);

      g_clear_error (&error);
    }

  g_object_notify (user_data, "goa-client-ready");
  g_object_notify (user_data, "goa-client");
}

static void
gtd_manager__commit_source_finished (GObject      *registry,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GtdManagerPrivate *priv = GTD_MANAGER (user_data)->priv;
  GError *error = NULL;

  g_return_if_fail (GTD_IS_MANAGER (user_data));

  gtd_object_set_ready (GTD_OBJECT (user_data), TRUE);
  e_source_registry_commit_source_finish (E_SOURCE_REGISTRY (registry),
                                          result,
                                          &error);

  if (error)
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error saving task list"),
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
gtd_manager__remove_source_finished (GObject      *source,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GtdManagerPrivate *priv = GTD_MANAGER (user_data)->priv;
  GError *error = NULL;

  g_return_if_fail (GTD_IS_MANAGER (user_data));

  gtd_object_set_ready (GTD_OBJECT (user_data), TRUE);
  e_source_remove_finish (E_SOURCE (source),
                          result,
                          &error);

  if (error)
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error removing task list"),
                 error->message);

      g_error_free (error);
      return;
    }
  else
    {
      priv->task_lists = g_list_remove (priv->task_lists, source);
    }
}

static void
gtd_manager__create_task_finished (GObject      *client,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  gboolean success;
  gchar *new_uid = NULL;
  GError *error = NULL;

  success = e_cal_client_create_object_finish (E_CAL_CLIENT (client),
                                               result,
                                               &new_uid,
                                               &error);

  gtd_object_set_ready (GTD_OBJECT (user_data), TRUE);

  if (error)
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error creating task"),
                 error->message);

      g_error_free (error);
      return;
    }
  else if (new_uid)
    {
      gtd_object_set_uid (GTD_OBJECT (user_data), new_uid);
      g_free (new_uid);
    }
}

static void
gtd_manager__remove_task_finished (GObject      *client,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  gboolean success;
  GError *error = NULL;

  success = e_cal_client_remove_object_finish (E_CAL_CLIENT (client),
                                               result,
                                               &error);

  gtd_object_set_ready (GTD_OBJECT (user_data), TRUE);
  g_object_unref (user_data);

  if (error)
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error removing task"),
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
gtd_manager__update_task_finished (GObject      *client,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  gboolean success;
  GError *error = NULL;

  success = e_cal_client_modify_object_finish (E_CAL_CLIENT (client),
                                               result,
                                               &error);

  gtd_object_set_ready (GTD_OBJECT (user_data), TRUE);

  if (error)
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error updating task"),
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
gtd_manager__invoke_authentication (GObject      *source_object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  ESource *source = E_SOURCE (source_object);
  GError *error = NULL;
  gboolean canceled;

  e_source_invoke_authenticate_finish (source,
                                       result,
                                       &error);

  canceled = g_error_matches (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_CANCELLED);

  if (!canceled)
    {
      g_warning ("%s: %s (%s): %s",
                 G_STRFUNC,
                 _("Failed to prompt for credentials"),
                 e_source_get_uid (source),
                 error->message);
    }

  g_clear_error (&error);
}

static void
gtd_manager__credentials_prompt_done (GObject      *source_object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  ETrustPromptResponse response = E_TRUST_PROMPT_RESPONSE_UNKNOWN;
  ESource *source = E_SOURCE (source_object);
  GError *error = NULL;

  e_trust_prompt_run_for_source_finish (source, result, &response, &error);

  if (error)
    {
      g_warning ("%s: %s '%s': %s",
                 G_STRFUNC,
                 _("Failed to prompt for credentials for"),
                 e_source_get_display_name (source),
                 error->message);

    }
  else if (response == E_TRUST_PROMPT_RESPONSE_ACCEPT || response == E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY)
    {
      /* Use NULL credentials to reuse those from the last time. */
      e_source_invoke_authenticate (source,
                                    NULL,
                                    NULL /* cancellable */,
                                    gtd_manager__invoke_authentication,
                                    NULL);
    }

  g_clear_error (&error);
}

static void
gtd_manager__credentials_required (ESourceRegistry          *registry,
                                   ESource                  *source,
                                   ESourceCredentialsReason  reason,
                                   const gchar              *certificate_pem,
                                   GTlsCertificateFlags      certificate_errors,
                                   const GError             *error,
                                   gpointer                  user_data)
{
  GtdManagerPrivate *priv;

  g_return_if_fail (GTD_IS_MANAGER (user_data));

  priv = GTD_MANAGER (user_data)->priv;

  if (e_credentials_prompter_get_auto_prompt_disabled_for (priv->credentials_prompter, source))
    return;

  if (reason == E_SOURCE_CREDENTIALS_REASON_SSL_FAILED)
    {
      e_trust_prompt_run_for_source (e_credentials_prompter_get_dialog_parent (priv->credentials_prompter),
                                     source,
                                     certificate_pem,
                                     certificate_errors,
                                     error ? error->message : NULL,
                                     TRUE, // allow saving sources
                                     NULL, // we won't cancel the operation
                                     gtd_manager__credentials_prompt_done,
                                     NULL);
    }
  else if (error && reason == E_SOURCE_CREDENTIALS_REASON_ERROR)
    {
      g_warning ("%s: %s '%s': %s",
                 G_STRFUNC,
                 _("Authentication failure"),
                 e_source_get_display_name (source),
                 error->message);
    }
}

static void
gtd_manager__fill_task_list (GObject      *client,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GSList *component_list;
  GError *error = NULL;

  e_cal_client_get_object_list_as_comps_finish (E_CAL_CLIENT (client),
                                                result,
                                                &component_list,
                                                &error);

  gtd_object_set_ready (GTD_OBJECT (user_data), TRUE);

  if (!error)
    {
      GSList *l;

      for (l = component_list; l != NULL; l = l->next)
        {
          GtdTask *task;

          task = gtd_task_new (l->data);
          gtd_task_set_list (task, GTD_TASK_LIST (user_data));

          gtd_task_list_save_task (GTD_TASK_LIST (user_data), task);
        }

      e_cal_client_free_ecalcomp_slist (component_list);
    }
  else
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error fetching tasks from list"),
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
gtd_manager__on_client_connected (GObject      *source_object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GtdManagerPrivate *priv = GTD_MANAGER (user_data)->priv;
  ECalClient *client;
  ESource *source;
  GError *error = NULL;

  source = e_client_get_source (E_CLIENT (source_object));
  client = E_CAL_CLIENT (e_cal_client_connect_finish (result, &error));

  /* Update ready flag */
  priv->load_sources--;
  gtd_object_set_ready (GTD_OBJECT (user_data),
                        priv->load_sources == 0);

  if (!error)
    {
      ESource *parent;
      GtdTaskList *list;

      /* parent source's display name is list's origin */
      parent = e_source_registry_ref_source (priv->source_registry, e_source_get_parent (source));

      /* creates a new task list */
      list = gtd_task_list_new (source, e_source_get_display_name (parent));

      /* it's not ready until we fetch the list of tasks from client */
      gtd_object_set_ready (GTD_OBJECT (list), FALSE);

      /* asyncronously fetch the task list */
      e_cal_client_get_object_list_as_comps (client,
                                             "contains? \"any\" \"\"",
                                             NULL,
                                             (GAsyncReadyCallback) gtd_manager__fill_task_list,
                                             list);


      priv->task_lists = g_list_append (priv->task_lists, list);

      g_object_set_data (G_OBJECT (source), "task-list", list);
      g_hash_table_insert (priv->clients, source, client);

      g_signal_emit (user_data,
                     signals[LIST_ADDED],
                     0,
                     list);

      g_object_unref (parent);

      g_debug ("%s: %s (%s)",
               G_STRFUNC,
               _("Task list source successfully connected"),
               e_source_get_display_name (source));
    }
  else
    {
      g_debug ("%s: %s (%s): %s",
               G_STRFUNC,
               _("Failed to connect to task list source"),
               e_source_get_uid (source),
               error->message);

      g_error_free (error);
      return;
    }

}

static void
gtd_manager__load__source (GtdManager *manager,
                           ESource    *source)
{
  GtdManagerPrivate *priv = manager->priv;

  if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST) &&
      !g_hash_table_lookup (priv->clients, source))
    {
      e_cal_client_connect (source,
                            E_CAL_CLIENT_SOURCE_TYPE_TASKS,
                            5, /* seconds to wait */
                            NULL,
                            gtd_manager__on_client_connected,
                            manager);
    }
  else
    {
      g_warning ("%s: %s %s (%s)",
                 G_STRFUNC,
                 _("Skipping already loaded task list "),
                 e_source_get_display_name (source),
                 e_source_get_uid (source));
    }
}

static void
gtd_manager__remove_source (GtdManager *manager,
                            ESource    *source)
{
  GtdManagerPrivate *priv = manager->priv;
  GtdTaskList *list;

  list = g_object_get_data (G_OBJECT (source), "task-list");

  g_hash_table_remove (priv->clients, source);

  g_signal_emit (manager,
                 signals[LIST_REMOVED],
                 0,
                 list);
}

static void
gtd_manager__source_registry_finish_cb (GObject      *source_object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GtdManagerPrivate *priv = GTD_MANAGER (user_data)->priv;
  GList *l;
  GList *sources;
  GError *error = NULL;

  priv->source_registry = e_source_registry_new_finish (result, &error);
  priv->credentials_prompter = e_credentials_prompter_new (priv->source_registry);

  if (error != NULL)
    {
      g_warning ("%s: %s", _("Error loading task manager"), error->message);
      g_error_free (error);
      return;
    }

  /* First of all, disable authentication dialog for non-tasklists sources */
  sources = e_source_registry_list_sources (priv->source_registry, NULL);

  for (l = sources; l != NULL; l = g_list_next (l))
    {
      ESource *source = E_SOURCE (l->data);

      /* Mark for skip also currently disabled sources */
      e_credentials_prompter_set_auto_prompt_disabled_for (priv->credentials_prompter,
                                                           source,
                                                           !e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR));
    }

  g_list_free_full (sources, g_object_unref);

  /* Load task list sources */
  sources = e_source_registry_list_sources (priv->source_registry,
                                            E_SOURCE_EXTENSION_TASK_LIST);

  /* While load_sources > 0, GtdManager::ready = FALSE */
  priv->load_sources = g_list_length (sources);

  g_debug ("%s: number of sources to load: %d",
           G_STRFUNC,
           priv->load_sources);

  gtd_object_set_ready (GTD_OBJECT (user_data),
                        priv->load_sources == 0);

  for (l = sources; l != NULL; l = l->next)
    gtd_manager__load__source (GTD_MANAGER (user_data), l->data);

  g_list_free_full (sources, g_object_unref);

  /* listen to the signals, so new sources don't slip by */
  g_signal_connect_swapped (priv->source_registry,
                            "source-added",
                            G_CALLBACK (gtd_manager__load__source),
                            user_data);

  g_signal_connect_swapped (priv->source_registry,
                            "source-removed",
                            G_CALLBACK (gtd_manager__remove_source),
                            user_data);

  g_signal_connect (priv->source_registry,
                    "credentials-required",
                    G_CALLBACK (gtd_manager__credentials_required),
                    user_data);

  e_credentials_prompter_process_awaiting_credentials (priv->credentials_prompter);
}

static void
gtd_manager_finalize (GObject *object)
{
  GtdManager *self = (GtdManager *)object;

  g_clear_object (&self->priv->goa_client);

  G_OBJECT_CLASS (gtd_manager_parent_class)->finalize (object);
}

static void
gtd_manager_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtdManager *self = GTD_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_manager_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtdManager *self = GTD_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_manager_constructed (GObject *object)
{
  GtdManagerPrivate *priv = GTD_MANAGER (object)->priv;
  GtdStorage *local_storage;
  gchar *default_location;

  G_OBJECT_CLASS (gtd_manager_parent_class)->constructed (object);

  default_location = g_settings_get_string (priv->settings, "storage-location");

  /* hash table */
  priv->clients = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                         (GEqualFunc) e_source_equal,
                                         g_object_unref,
                                         g_object_unref);

  /* load the source registry */
  e_source_registry_new (NULL,
                         (GAsyncReadyCallback) gtd_manager__source_registry_finish_cb,
                         object);

  /* local storage location */
  local_storage = gtd_storage_new ("local",
                                   "local",
                                   _("Local"),
                                   _("On This Computer"));
  gtd_storage_set_enabled (local_storage, TRUE);
  gtd_storage_set_is_default (local_storage, g_strcmp0 (default_location, "local") == 0);

  priv->storage_locations = g_list_append (priv->storage_locations, local_storage);

  /* online accounts */
  goa_client_new (NULL,
                  (GAsyncReadyCallback) gtd_manager__goa_client_finish_cb,
                  object);

  g_free (default_location);
}

static void
gtd_manager_class_init (GtdManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtd_manager_finalize;
  object_class->get_property = gtd_manager_get_property;
  object_class->set_property = gtd_manager_set_property;
  object_class->constructed = gtd_manager_constructed;

  /**
   * GtdManager::goa-client:
   *
   * The #GoaClient asyncronously loaded.
   */
  g_object_class_install_property (
        object_class,
        PROP_GOA_CLIENT,
        g_param_spec_object ("goa-client",
                            _("The online accounts client of the manager"),
                            _("The read-only GNOME online accounts client loaded and owned by the manager"),
                            GOA_TYPE_CLIENT,
                            G_PARAM_READABLE));

  /**
   * GtdManager::goa-client-ready:
   *
   * Whether the GNOME Online Accounts client is loaded.
   */
  g_object_class_install_property (
        object_class,
        PROP_GOA_CLIENT_READY,
        g_param_spec_boolean ("goa-client-ready",
                              _("Whether GNOME Online Accounts client is ready"),
                              _("Whether the read-only GNOME online accounts client is loaded"),
                              FALSE,
                              G_PARAM_READABLE));

  /**
   * GtdManager::source-registry:
   *
   * The #ESourceRegistry asyncronously loaded.
   */
  g_object_class_install_property (
        object_class,
        PROP_SOURCE_REGISTRY,
        g_param_spec_object ("source-registry",
                            _("The source registry of the manager"),
                            _("The read-only source registry loaded and owned by the manager"),
                            E_TYPE_SOURCE_REGISTRY,
                            G_PARAM_READABLE));

  /**
   * GtdManager::list-added:
   *
   * The ::list-added signal is emmited after a #GtdTaskList
   * is connected.
   */
  signals[LIST_ADDED] = g_signal_new ("list-added",
                                      GTD_TYPE_MANAGER,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL,
                                      NULL,
                                      NULL,
                                      G_TYPE_NONE,
                                      1,
                                      GTD_TYPE_TASK_LIST);

/**
   * GtdManager::list-changed:
   *
   * The ::list-changed signal is emmited after a #GtdTaskList
   * has any of it's properties changed.
   */
  signals[LIST_CHANGED] = g_signal_new ("list-changed",
                                        GTD_TYPE_MANAGER,
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL,
                                        NULL,
                                        NULL,
                                        G_TYPE_NONE,
                                        1,
                                        GTD_TYPE_TASK_LIST);

  /**
   * GtdManager::list-removed:
   *
   * The ::list-removed signal is emmited after a #GtdTaskList
   * is disconnected.
   */
  signals[LIST_REMOVED] = g_signal_new ("list-removed",
                                        GTD_TYPE_MANAGER,
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL,
                                        NULL,
                                        NULL,
                                        G_TYPE_NONE,
                                        1,
                                        GTD_TYPE_TASK_LIST);

  /**
   * GtdManager::storage-added:
   *
   * The ::storage-added signal is emmited after a #GtdStorage
   * is added.
   */
  signals[STORAGE_ADDED] = g_signal_new ("storage-added",
                                         GTD_TYPE_MANAGER,
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL,
                                         NULL,
                                         NULL,
                                         G_TYPE_NONE,
                                         1,
                                         GTD_TYPE_STORAGE);

  /**
   * GtdManager::storage-changed:
   *
   * The ::storage-changed signal is emmited after a #GtdStorage
   * is changed.
   */
  signals[STORAGE_CHANGED] = g_signal_new ("storage-changed",
                                           GTD_TYPE_MANAGER,
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL,
                                          NULL,
                                          NULL,
                                          G_TYPE_NONE,
                                          1,
                                          GTD_TYPE_STORAGE);

  /**
   * GtdManager::storage-removed:
   *
   * The ::storage-removed signal is emmited after a #GtdStorage
   * is removed from the list.
   */
  signals[STORAGE_REMOVED] = g_signal_new ("storage-removed",
                                           GTD_TYPE_MANAGER,
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL,
                                           NULL,
                                           NULL,
                                           G_TYPE_NONE,
                                           1,
                                           GTD_TYPE_STORAGE);
}

static void
gtd_manager_init (GtdManager *self)
{
  self->priv = gtd_manager_get_instance_private (self);
  self->priv->settings = g_settings_new ("org.gnome.todo");
}

GtdManager*
gtd_manager_new (void)
{
  return g_object_new (GTD_TYPE_MANAGER, NULL);
}

/**
 * gtd_manager_create_task:
 * @manager: a #GtdManager
 * @task: a #GtdTask
 *
 * Ask for @task's parent list source to create @task.
 *
 * Returns:
 */
void
gtd_manager_create_task (GtdManager *manager,
                         GtdTask    *task)
{
  GtdManagerPrivate *priv = GTD_MANAGER (manager)->priv;
  ECalComponent *component;
  ECalClient *client;
  ESource *source;

  g_return_if_fail (GTD_IS_MANAGER (manager));
  g_return_if_fail (GTD_IS_TASK (task));

  source = gtd_task_list_get_source (gtd_task_get_list (task));
  client = g_hash_table_lookup (priv->clients, source);
  component = gtd_task_get_component (task);

  /* The task is not ready until we finish the operation */
  gtd_object_set_ready (GTD_OBJECT (task), FALSE);

  e_cal_client_create_object (client,
                              e_cal_component_get_icalcomponent (component),
                              NULL, // We won't cancel the operation
                              (GAsyncReadyCallback) gtd_manager__create_task_finished,
                              task);
}

/**
 * gtd_manager_remove_task:
 * @manager: a #GtdManager
 * @task: a #GtdTask
 *
 * Ask for @task's parent list source to remove @task.
 *
 * Returns:
 */
void
gtd_manager_remove_task (GtdManager *manager,
                         GtdTask    *task)
{
  GtdManagerPrivate *priv = GTD_MANAGER (manager)->priv;
  ECalComponent *component;
  ECalComponentId *id;
  ECalClient *client;
  ESource *source;

  g_return_if_fail (GTD_IS_MANAGER (manager));
  g_return_if_fail (GTD_IS_TASK (task));

  source = gtd_task_list_get_source (gtd_task_get_list (task));
  client = g_hash_table_lookup (priv->clients, source);
  component = gtd_task_get_component (task);
  id = e_cal_component_get_id (component);

  /* The task is not ready until we finish the operation */
  gtd_object_set_ready (GTD_OBJECT (task), FALSE);

  e_cal_client_remove_object (client,
                              id->uid,
                              id->rid,
                              E_CAL_OBJ_MOD_THIS,
                              NULL, // We won't cancel the operation
                              (GAsyncReadyCallback) gtd_manager__remove_task_finished,
                              task);

  e_cal_component_free_id (id);
}

/**
 * gtd_manager_update_task:
 * @manager: a #GtdManager
 * @task: a #GtdTask
 *
 * Ask for @task's parent list source to update @task.
 *
 * Returns:
 */
void
gtd_manager_update_task (GtdManager *manager,
                         GtdTask    *task)
{
  GtdManagerPrivate *priv = GTD_MANAGER (manager)->priv;
  ECalComponent *component;
  ECalClient *client;
  ESource *source;

  g_return_if_fail (GTD_IS_MANAGER (manager));
  g_return_if_fail (GTD_IS_TASK (task));

  source = gtd_task_list_get_source (gtd_task_get_list (task));
  client = g_hash_table_lookup (priv->clients, source);
  component = gtd_task_get_component (task);

  /* The task is not ready until we finish the operation */
  gtd_object_set_ready (GTD_OBJECT (task), FALSE);

  e_cal_client_modify_object (client,
                              e_cal_component_get_icalcomponent (component),
                              E_CAL_OBJ_MOD_THIS,
                              NULL, // We won't cancel the operation
                              (GAsyncReadyCallback) gtd_manager__update_task_finished,
                              task);
}

/**
 * gtd_manager_remove_task_list:
 * @manager: a #GtdManager
 * @list: a #GtdTaskList
 *
 * Deletes @list from the registry.
 *
 * Returns:
 */
void
gtd_manager_remove_task_list (GtdManager  *manager,
                              GtdTaskList *list)
{
  ESource *source;

  g_return_if_fail (GTD_IS_MANAGER (manager));
  g_return_if_fail (GTD_IS_TASK_LIST (list));
  g_return_if_fail (gtd_task_list_get_source (list));

  source = gtd_task_list_get_source (list);

  gtd_object_set_ready (GTD_OBJECT (manager), FALSE);
  e_source_remove (source,
                   NULL,
                   (GAsyncReadyCallback) gtd_manager__remove_source_finished,
                   manager);
}

/**
 * gtd_manager_save_task_list:
 * @manager: a #GtdManager
 * @list: a #GtdTaskList
 *
 * Save or create @list.
 *
 * Returns:
 */
void
gtd_manager_save_task_list (GtdManager  *manager,
                            GtdTaskList *list)
{
  ESource *source;

  g_return_if_fail (GTD_IS_MANAGER (manager));
  g_return_if_fail (GTD_IS_TASK_LIST (list));
  g_return_if_fail (gtd_task_list_get_source (list));

  source = gtd_task_list_get_source (list);

  gtd_object_set_ready (GTD_OBJECT (manager), FALSE);
  e_source_registry_commit_source (manager->priv->source_registry,
                                   source,
                                   NULL,
                                   (GAsyncReadyCallback) gtd_manager__commit_source_finished,
                                   manager);
}

/**
 * gtd_manager_get_goa_client:
 * @manager: a #GtdManager
 *
 * Retrieves the internal @GoaClient from @manager. %NULL doesn't mean
 * that the client is not ready, use @gtd_manager_is_goa_client_ready to
 * check that.
 *
 * Returns: (transfer none): the internal #GoaClient of @manager
 */
GoaClient*
gtd_manager_get_goa_client (GtdManager *manager)
{
  g_return_val_if_fail (GTD_IS_MANAGER (manager), NULL);

  return manager->priv->goa_client;
}

/**
 * gtd_manager_is_goa_client_ready:
 *
 * Checks whether @manager's internal #GoaClient is ready. Note that,
 * in the case of failure, it'll return %TRUE and @gtd_manager_get_goa_client
 * will return %NULL.
 *
 * Returns: %TRUE if @manager's internal #GoaClient is already
 * loaded, %FALSE otherwise.
 */
gboolean
gtd_manager_is_goa_client_ready (GtdManager *manager)
{
  g_return_val_if_fail (GTD_IS_MANAGER (manager), FALSE);

  return manager->priv->goa_client_ready;
}

/**
 * gtd_manager_get_task_lists:
 * @manager: a #GtdManager
 *
 * Retrieves the list of #GtdTaskList already loaded.
 *
 * Returns: (transfer full): a newly allocated list of #GtdTaskList, or %NULL if none.
 */
GList*
gtd_manager_get_task_lists (GtdManager *manager)
{
  g_return_val_if_fail (GTD_IS_MANAGER (manager), NULL);

  return g_list_copy (manager->priv->task_lists);
}

/**
 * gtd_manager_get_storage_locations:
 *
 * Retrieves the list of available #GtdStorage.
 *
 * Returns: (transfer full): (type #GtdStorage): a newly allocated #GList of
 * #GtdStorage. Free with @g_list_free after use.
 */
GList*
gtd_manager_get_storage_locations (GtdManager *manager)
{
  g_return_val_if_fail (GTD_IS_MANAGER (manager), NULL);

  return g_list_copy (manager->priv->storage_locations);
}
