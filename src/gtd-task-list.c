/* gtd-task-list.c
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

#include "gtd-task.h"
#include "gtd-task-list.h"

#include <glib/gi18n.h>
#include <libecal/libecal.h>

typedef struct
{
  GList               *tasks;
  ESource             *source;
  gchar               *origin;
} GtdTaskListPrivate;

struct _GtdTaskList
{
  GtdObject parent;

  /*<private>*/
  GtdTaskListPrivate *priv;
};

enum
{
  TASK_ADDED,
  TASK_REMOVED,
  TASK_UPDATED,
  NUM_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdTaskList, gtd_task_list, GTD_TYPE_OBJECT)

static guint signals[NUM_SIGNALS] = { 0, };

enum
{
  PROP_0,
  PROP_COLOR,
  PROP_NAME,
  PROP_ORIGIN,
  PROP_SOURCE,
  LAST_PROP
};

static void
gtd_task_list_finalize (GObject *object)
{
  GtdTaskList *self = (GtdTaskList*) object;

  G_OBJECT_CLASS (gtd_task_list_parent_class)->finalize (object);
}

static void
gtd_task_list_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtdTaskList *self = GTD_TASK_LIST (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      g_value_set_boxed (value, gtd_task_list_get_color (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, e_source_get_display_name (self->priv->source));
      break;

    case PROP_ORIGIN:
      g_value_set_string (value, self->priv->origin);
      break;

    case PROP_SOURCE:
      g_value_set_object (value, self->priv->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_task_list_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtdTaskList *self = GTD_TASK_LIST (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      gtd_task_list_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_NAME:
      gtd_task_list_set_name (self, g_value_get_string (value));
      break;

    case PROP_ORIGIN:
      self->priv->origin = g_strdup (g_value_get_string (value));
      break;

    case PROP_SOURCE:
      self->priv->source = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_task_list_class_init (GtdTaskListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtd_task_list_finalize;
  object_class->get_property = gtd_task_list_get_property;
  object_class->set_property = gtd_task_list_set_property;

  /**
   * GtdTaskList::color:
   *
   * The color of the list.
   */
  g_object_class_install_property (
        object_class,
        PROP_COLOR,
        g_param_spec_boxed ("color",
                            _("Color of the list"),
                            _("The color of the list"),
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE));

  /**
   * GtdTaskList::name:
   *
   * The display name of the list.
   */
  g_object_class_install_property (
        object_class,
        PROP_NAME,
        g_param_spec_string ("name",
                             _("Name of the list"),
                             _("The name of the list"),
                             NULL,
                             G_PARAM_READWRITE));

  /**
   * GtdTaskList::name:
   *
   * The display name of the list.
   */
  g_object_class_install_property (
        object_class,
        PROP_ORIGIN,
        g_param_spec_string ("origin",
                             _("Data origin of the list"),
                             _("The data origin location of the list"),
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtdTaskList::source:
   *
   * The parent source of the list.
   */
  g_object_class_install_property (
        object_class,
        PROP_SOURCE,
        g_param_spec_object ("source",
                             _("Source of the list"),
                             _("The parent source that handles the list"),
                             E_TYPE_SOURCE,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtdTaskList::task-added:
   *
   * The ::task-added signal is emmited after a #GtdTask
   * is added to the list.
   */
  signals[TASK_ADDED] = g_signal_new ("task-added",
                                      GTD_TYPE_TASK_LIST,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL,
                                      NULL,
                                      NULL,
                                      G_TYPE_NONE,
                                      1,
                                      GTD_TYPE_TASK);

  /**
   * GtdTaskList::task-removed:
   *
   * The ::task-removed signal is emmited after a #GtdTask
   * is removed from the list.
   */
  signals[TASK_REMOVED] = g_signal_new ("task-removed",
                                        GTD_TYPE_TASK_LIST,
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL,
                                        NULL,
                                        NULL,
                                        G_TYPE_NONE,
                                        1,
                                        GTD_TYPE_TASK);

  /**
   * GtdTaskList::task-updated:
   *
   * The ::task-updated signal is emmited after a #GtdTask
   * in the list is updated.
   */
  signals[TASK_UPDATED] = g_signal_new ("task-updated",
                                      GTD_TYPE_TASK_LIST,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL,
                                      NULL,
                                      NULL,
                                      G_TYPE_NONE,
                                      1,
                                      GTD_TYPE_TASK);
}

static void
gtd_task_list_init (GtdTaskList *self)
{
  self->priv = gtd_task_list_get_instance_private (self);
}

/**
 * gtd_task_list_new:
 * @uid: the unique identifier of the list, usually set
 * by the backend
 *
 * Creates a new list.
 *
 * Returns: (transfer full): the new #GtdTaskList
 */
GtdTaskList *
gtd_task_list_new (ESource     *source,
                   const gchar *origin)
{
  return g_object_new (GTD_TYPE_TASK_LIST,
                       "source", source,
                       "uid", e_source_get_uid (source),
                       "origin", origin,
                       NULL);
}

/**
 * gtd_task_list_get_color:
 * @list: a #GtdTaskList
 *
 * Retrieves the color of %list. It is guarantee that it always returns a
 * color, given a valid #GtdTaskList.
 *
 * Returns: (transfer full): the color of %list. Free with %gdk_rgba_free after use.
 */
GdkRGBA*
gtd_task_list_get_color (GtdTaskList *list)
{
  ESourceSelectable *selectable;
  GdkRGBA color;

  g_return_val_if_fail (GTD_IS_TASK_LIST (list), NULL);
  g_return_val_if_fail (E_IS_SOURCE (list->priv->source), NULL);

  selectable = E_SOURCE_SELECTABLE (e_source_get_extension (list->priv->source, E_SOURCE_EXTENSION_CALENDAR));

  if (!gdk_rgba_parse (&color, e_source_selectable_get_color (selectable)))
    gdk_rgba_parse (&color, "#ffffff"); /* calendar default colour */

  return gdk_rgba_copy (&color);
}

void
gtd_task_list_set_color (GtdTaskList   *list,
                         const GdkRGBA *color)
{
  GdkRGBA *current_color;

  g_return_if_fail (GTD_IS_TASK_LIST (list));

  current_color = gtd_task_list_get_color (list);

  if (!gdk_rgba_equal (current_color, color))
    {
      ESourceSelectable *selectable;
      gchar *color_str;

      selectable = E_SOURCE_SELECTABLE (e_source_get_extension (list->priv->source, E_SOURCE_EXTENSION_CALENDAR));
      color_str = gdk_rgba_to_string (color);

      e_source_selectable_set_color (selectable, color_str);
      g_free (color_str);

      g_object_notify (G_OBJECT (list), "color");
    }

  gdk_rgba_free (current_color);
}

/**
 * gtd_task_list_get_name:
 * @list: a #GtdTaskList
 *
 * Retrieves the user-visible name of @list, or %NULL.
 *
 * Returns: (transfer none): the internal name of @list. Do not free
 * after use.
 */
const gchar*
gtd_task_list_get_name (GtdTaskList *list)
{
  g_return_val_if_fail (GTD_IS_TASK_LIST (list), NULL);

  return e_source_get_display_name (list->priv->source);
}

/**
 * gtd_task_list_set_name:
 *
 * Sets the @list name to @name.
 *
 * Returns:
 */
void
gtd_task_list_set_name (GtdTaskList *list,
                        const gchar *name)
{
  g_assert (GTD_IS_TASK_LIST (list));

  if (g_strcmp0 (e_source_get_display_name (list->priv->source), name) != 0)
    {
      e_source_set_display_name (list->priv->source, name);

      g_object_notify (G_OBJECT (list), "name");
    }
}

/**
 * gtd_task_list_get_tasks:
 * @list: a #GtdTaskList
 *
 * Returns the list's tasks.
 *
 * Returns: (element-type GtdTask) (transfer container): a newly-allocated list of the list's tasks.
 */
GList*
gtd_task_list_get_tasks (GtdTaskList *list)
{
  g_return_val_if_fail (GTD_IS_TASK_LIST (list), NULL);

  return g_list_copy (list->priv->tasks);
}

/**
 * gtd_task_list_save_task:
 * @list: a #GtdTaskList
 * @task: a #GtdTask
 *
 * Adds or updates @task to @list if it's not already present.
 *
 * Returns:
 */
void
gtd_task_list_save_task (GtdTaskList *list,
                         GtdTask     *task)
{
  g_assert (GTD_IS_TASK_LIST (list));
  g_assert (GTD_IS_TASK (task));

  if (gtd_task_list_contains (list, task))
    {
      g_signal_emit (list, signals[TASK_UPDATED], 0, task);
    }
  else
    {
      list->priv->tasks = g_list_append (list->priv->tasks, task);

      g_signal_emit (list, signals[TASK_ADDED], 0, task);
    }
}

/**
 * gtd_task_list_remove_task:
 * @list: a #GtdTaskList
 * @task: a #GtdTask
 *
 * Removes @task from @list if it's inside the list.
 *
 * Returns:
 */
void
gtd_task_list_remove_task (GtdTaskList *list,
                           GtdTask     *task)
{
  g_assert (GTD_IS_TASK_LIST (list));
  g_assert (GTD_IS_TASK (task));

  if (!gtd_task_list_contains (list, task))
    return;

  list->priv->tasks = g_list_remove (list->priv->tasks, task);

  g_signal_emit (list, signals[TASK_REMOVED], 0, task);
}

/**
 * gtd_task_list_contains:
 * @list: a #GtdTaskList
 * @task: a #GtdTask
 *
 * Checks if @task is inside @list.
 *
 * Returns: %TRUE if @list contains @task, %FALSE otherwise
 */
gboolean
gtd_task_list_contains (GtdTaskList *list,
                        GtdTask     *task)
{
  g_assert (GTD_IS_TASK_LIST (list));
  g_assert (GTD_IS_TASK (task));

  return (g_list_find (list->priv->tasks, task) != NULL);
}

/**
 * gtd_task_list_get_source:
 * @list: a #GtdTaskList
 *
 * Retrieves the #ESource that handles @list.
 *
 * Returns: (transfer none): the internal #ESource of @list
 */
ESource*
gtd_task_list_get_source (GtdTaskList *list)
{
  g_return_val_if_fail (GTD_IS_TASK_LIST (list), NULL);

  return list->priv->source;
}

/**
 * gtd_task_list_get_origin:
 * @list: a @GtdTaskList
 *
 * Gets the origin (i.e. parent source display name) of @list.
 *
 * Returns: (transfer none): the origin of @list data.
 */
const gchar*
gtd_task_list_get_origin (GtdTaskList *list)
{
  g_return_val_if_fail (GTD_IS_TASK_LIST (list), NULL);

  return list->priv->origin;
}
