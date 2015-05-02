/* gtd-list-view.c
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

#include "gtd-list-view.h"
#include "gtd-manager.h"
#include "gtd-task-list.h"
#include "gtd-task-row.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

typedef struct
{
  GtkListBox            *listbox;
  GtdTaskRow            *new_task_row;

  /* internal */
  gboolean               readonly;
  GList                 *list;
  GtdTaskList           *task_list;
  GtdManager            *manager;
} GtdListViewPrivate;

struct _GtdListView
{
  GtkBox               parent;

  /*<private>*/
  GtdListViewPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdListView, gtd_list_view, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_MANAGER,
  PROP_READONLY,
  LAST_PROP
};

static void
gtd_list_view_finalize (GObject *object)
{
  GtdListView *self = (GtdListView *)object;
  GtdListViewPrivate *priv = gtd_list_view_get_instance_private (self);

  G_OBJECT_CLASS (gtd_list_view_parent_class)->finalize (object);
}

static void
gtd_list_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtdListView *self = GTD_LIST_VIEW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->priv->manager);
      break;

    case PROP_READONLY:
      g_value_set_boolean (value, self->priv->readonly);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_list_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtdListView *self = GTD_LIST_VIEW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->manager = g_value_get_object (value);
      break;

    case PROP_READONLY:
      gtd_list_view_set_readonly (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_list_view_class_init (GtdListViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_list_view_finalize;
  object_class->get_property = gtd_list_view_get_property;
  object_class->set_property = gtd_list_view_set_property;

  /**
   * GtdListView::manager:
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
   * GtdListView::readonly:
   *
   * Whether the list shows the "New Task" row or not.
   */
  g_object_class_install_property (
        object_class,
        PROP_READONLY,
        g_param_spec_boolean ("readonly",
                              _("Whether the list is readonly"),
                              _("Whether the list is readonly, i.e. doesn't show the New Task row, or not"),
                              FALSE,
                              G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/list-view.ui");
}

static void
gtd_list_view_init (GtdListView *self)
{
  self->priv = gtd_list_view_get_instance_private (self);

  self->priv->new_task_row = GTD_TASK_ROW (gtd_task_row_new (NULL));

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gtd_list_view_new:
 *
 * Creates a new #GtdListView
 *
 * Returns: a newly allocated #GtdListView
 */
GtkWidget*
gtd_list_view_new (void)
{
  return g_object_new (GTD_TYPE_LIST_VIEW, NULL);
}

/**
 * gtd_list_view_get_list:
 * @view: a #GtdListView
 *
 * Retrieves the list of tasks from @view. Note that,
 * if a #GtdTaskList is set, the #GtdTaskList's list
 * of task will be returned.
 *
 * Returns: (element-type GtdTaskList) (transfer full): the internal list of
 * tasks. Free with @g_list_free after use.
 */
GList*
gtd_list_view_get_list (GtdListView *view)
{
  g_return_val_if_fail (GTD_IS_LIST_VIEW (view), NULL);

  if (view->priv->task_list)
    return gtd_task_list_get_tasks (view->priv->task_list);
  else if (view->priv->list)
    return g_list_copy (view->priv->list);
  else
    return NULL;
}

/**
 * gtd_list_view_set_list:
 * @view: a #GtdListView
 *
 * Copies the tasks from @list to @view.
 *
 * Returns:
 */
void
gtd_list_view_set_list (GtdListView *view,
                        GList       *list)
{
  g_return_if_fail (GTD_IS_LIST_VIEW (view));

  if (view->priv->list)
    g_list_free (view->priv->list);

  view->priv->list = g_list_copy (list);
}

/**
 * gtd_list_view_get_manager:
 * @view: a #GtdListView
 *
 * Retrieves the #GtdManager from @view.
 *
 * Returns: (transfer none): the #GtdManager of @view
 */
GtdManager*
gtd_list_view_get_manager (GtdListView *view)
{
  g_return_val_if_fail (GTD_IS_LIST_VIEW (view), NULL);

  return view->priv->manager;
}

/**
 * gtd_list_view_set_manager:
 * @view: a #GtdListView
 * @manager: a #GtdManager
 *
 * Sets the #GtdManager of @view.
 *
 * Returns:
 */
void
gtd_list_view_set_manager (GtdListView *view,
                           GtdManager  *manager)
{
  g_return_if_fail (GTD_IS_LIST_VIEW (view));
  g_return_if_fail (GTD_IS_MANAGER (manager));

  if (view->priv->manager != manager)
    {
      view->priv->manager = manager;

      g_object_notify (G_OBJECT (view), "manager");
    }
}

/**
 * gtd_list_view_get_readonly:
 * @view: a #GtdListView
 *
 * Gets the readonly state of @view.
 *
 * Returns: %TRUE if @view is readonly, %FALSE otherwise
 */
gboolean
gtd_list_view_get_readonly (GtdListView *view)
{
  g_return_val_if_fail (GTD_IS_LIST_VIEW (view), FALSE);

  return view->priv->readonly;
}

/**
 * gtd_list_view_set_readonly:
 * @view: a #GtdListView
 *
 * Sets the GtdListView::readonly property of @view.
 *
 * Returns:
 */
void
gtd_list_view_set_readonly (GtdListView *view,
                            gboolean     readonly)
{
  g_return_if_fail (GTD_IS_LIST_VIEW (view));

  if (view->priv->readonly != readonly)
    {
      view->priv->readonly = readonly;

      /* Add/remove the new task row */
      if (gtk_widget_get_parent (GTK_WIDGET (view->priv->new_task_row)))
        {
          if (readonly)
            {
              gtk_container_remove (GTK_CONTAINER (view->priv->listbox), GTK_WIDGET (view->priv->new_task_row));
              gtk_widget_hide (GTK_WIDGET (view->priv->new_task_row));
            }
        }
      else
        {
          if (!readonly)
            {
              gtk_list_box_insert (view->priv->listbox,
                                   GTK_WIDGET (view->priv->new_task_row),
                                   -1);
              gtk_widget_show (GTK_WIDGET (view->priv->new_task_row));
            }
        }

      g_object_notify (G_OBJECT (view), "readonly");
    }
}

/**
 * gtd_list_view_get_task_list:
 * @view: a #GtdListView
 *
 * Retrieves the #GtdTaskList from @view, or %NULL if none was set.
 *
 * Returns: (transfer none): the @GtdTaskList of @view, or %NULL is
 * none was set.
 */
GtdTaskList*
gtd_list_view_get_task_list (GtdListView *view)
{
  g_return_val_if_fail (GTD_IS_LIST_VIEW (view), NULL);

  return view->priv->task_list;
}

/**
 * gtd_list_view_set_task_list:
 * @view: a #GtdListView
 * @list: a #GtdTaskList
 *
 * Sets the internal #GtdTaskList of @view.
 *
 * Returns:
 */
void
gtd_list_view_set_task_list (GtdListView *view,
                             GtdTaskList *list)
{
  g_return_if_fail (GTD_IS_LIST_VIEW (view));
  g_return_if_fail (GTD_IS_TASK_LIST (list));

  if (view->priv->task_list != list)
    view->priv->task_list = list;
}
