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
#include "gtd-task.h"
#include "gtd-task-list.h"

#include <glib/gi18n.h>
#include <libecal/libecal.h>

typedef struct
{
  GHashTable            *clients;
  ESourceRegistry       *source_registry;

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

enum
{
  LIST_ADDED,
  LIST_CHANGED,
  LIST_REMOVED,
  NUM_SIGNALS
};

enum
{
  PROP_0,
  PROP_SOURCE_REGISTRY,
  LAST_PROP
};

static guint signals[NUM_SIGNALS] = { 0, };

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
      GtdTaskList *list;

      /* creates a new task list */
      list = gtd_task_list_new (source);

      /* it's not ready until we fetch the list of tasks from client */
      gtd_object_set_ready (GTD_OBJECT (list), FALSE);

      /* asyncronously fetch the task list */
      e_cal_client_get_object_list_as_comps (client,
                                             "contains? \"any\" \"\"",
                                             NULL,
                                             (GAsyncReadyCallback) gtd_manager__fill_task_list,
                                             list);


      g_object_set_data (G_OBJECT (source), "task-list", list);
      g_hash_table_insert (priv->clients, source, client);

      g_signal_emit (user_data,
                     signals[LIST_ADDED],
                     0,
                     list);

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

  if (!g_hash_table_lookup (priv->clients, source))
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

  if (error != NULL)
    {
      g_warning ("%s: %s", _("Error loading task manager"), error->message);
      g_error_free (error);
      return;
    }

  /* Load task list sources */
  sources = e_source_registry_list_sources (priv->source_registry,
                                            E_SOURCE_EXTENSION_TASK_LIST);

  /* While load_sources > 0, GtdManager::ready = FALSE */
  priv->load_sources = g_list_length (sources);

  g_message ("%s: number of sources to load: %d",
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
}

static void
gtd_manager_finalize (GObject *object)
{
  GtdManager *self = (GtdManager *)object;
  GtdManagerPrivate *priv = gtd_manager_get_instance_private (self);

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

  G_OBJECT_CLASS (gtd_manager_parent_class)->constructed (object);

  /* hash table */
  priv->clients = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                         (GEqualFunc) e_source_equal,
                                         g_object_unref,
                                         g_object_unref);

  /* load the source registry */
  e_source_registry_new (NULL,
                         (GAsyncReadyCallback) gtd_manager__source_registry_finish_cb,
                         object);
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
}

static void
gtd_manager_init (GtdManager *self)
{
  self->priv = gtd_manager_get_instance_private (self);
}

GtdManager*
gtd_manager_new (void)
{
  return g_object_new (GTD_TYPE_MANAGER, NULL);
}
