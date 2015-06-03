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

#include "gtd-arrow-frame.h"
#include "gtd-edit-pane.h"
#include "gtd-list-view.h"
#include "gtd-manager.h"
#include "gtd-task.h"
#include "gtd-task-list.h"
#include "gtd-task-row.h"
#include "gtd-window.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

typedef struct
{
  GtdArrowFrame         *arrow_frame;
  GtdEditPane           *edit_pane;
  GtkRevealer           *edit_revealer;
  GtkListBox            *listbox;
  GtdTaskRow            *new_task_row;
  GtkRevealer           *revealer;
  GtkImage              *done_image;
  GtkLabel              *done_label;
  GtkScrolledWindow     *viewport;

  /* internal */
  gboolean               can_toggle;
  gint                   complete_tasks;
  gboolean               readonly;
  gboolean               show_list_name;
  gboolean               show_completed;
  GList                 *list;
  GtdTaskList           *task_list;
  GtdManager            *manager;

  /* color provider */
  GtkCssProvider        *color_provider;
} GtdListViewPrivate;

struct _GtdListView
{
  GtkOverlay          parent;

  /*<private>*/
  GtdListViewPrivate *priv;
};

#define COLOR_TEMPLATE "GtkViewport {background-color: %s;}"

#define TASK_REMOVED_NOTIFICATION_ID             "task-removed-id"

/* prototypes */
static void             gtd_list_view__task_completed                 (GObject          *object,
                                                                       GParamSpec       *spec,
                                                                       gpointer          user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GtdListView, gtd_list_view, GTK_TYPE_OVERLAY)

typedef struct
{
  GtdListView *view;
  GtdTask     *task;
} RemoveTaskData;

enum {
  PROP_0,
  PROP_MANAGER,
  PROP_READONLY,
  PROP_SHOW_COMPLETED,
  PROP_SHOW_LIST_NAME,
  LAST_PROP
};

static gboolean
remove_task_action (RemoveTaskData *data)
{
  g_return_val_if_fail (data != NULL, G_SOURCE_REMOVE);

  gtd_manager_remove_task (data->view->priv->manager, data->task);

  g_free (data);

  return G_SOURCE_REMOVE;
}

static gboolean
undo_remove_task_action (RemoveTaskData *data)
{
  GtdTaskList *list = gtd_task_get_list (data->task);

  g_return_val_if_fail (data != NULL, G_SOURCE_REMOVE);

  gtd_task_list_save_task (list, data->task);

  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
gtd_list_view__remove_task_cb (GtdEditPane *pane,
                               GtdTask     *task,
                               gpointer     user_data)
{
  GtdListViewPrivate *priv;
  RemoveTaskData *data;
  GtdWindow *window;
  gchar *text;

  g_return_if_fail (GTD_IS_LIST_VIEW (user_data));

  priv = GTD_LIST_VIEW (user_data)->priv;
  text = g_strdup_printf (_("Task <b>%s</b> removed"), gtd_task_get_title (task));
  window = GTD_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (user_data)));

  data = g_new0 (RemoveTaskData, 1);
  data->view = user_data;
  data->task = task;

  /* Remove the task from the list */
  gtd_task_list_remove_task (gtd_task_get_list (task), task);

  gtk_revealer_set_reveal_child (priv->edit_revealer, FALSE);

  gtd_window_notify (window,
                     7500, //ms
                     TASK_REMOVED_NOTIFICATION_ID,
                     text,
                     _("Undo"),
                     (GSourceFunc) remove_task_action,
                     (GSourceFunc) undo_remove_task_action,
                     FALSE,
                     data);

  g_free (text);
}

static void
gtd_list_view__edit_task_finished (GtdEditPane *pane,
                                   GtdTask     *task,
                                   gpointer     user_data)
{
  GtdListViewPrivate *priv;

  g_return_if_fail (GTD_IS_TASK (task));
  g_return_if_fail (GTD_IS_EDIT_PANE (pane));
  g_return_if_fail (GTD_IS_LIST_VIEW (user_data));

  priv = GTD_LIST_VIEW (user_data)->priv;

  gtk_revealer_set_reveal_child (priv->edit_revealer, FALSE);

  gtd_task_save (task);

  gtd_manager_update_task (priv->manager, task);
  gtd_task_list_save_task (priv->task_list, task);

  gtk_list_box_invalidate_sort (priv->listbox);
}

static void
gtd_list_view__color_changed (GObject    *object,
                              GParamSpec *spec,
                              gpointer    user_data)
{
  GtdListViewPrivate *priv = GTD_LIST_VIEW (user_data)->priv;
  GdkRGBA *color;
  gchar *color_str;
  gchar *parsed_css;

  /* Add the color to provider */
  color = gtd_task_list_get_color (GTD_TASK_LIST (object));
  color_str = gdk_rgba_to_string (color);

  parsed_css = g_strdup_printf (COLOR_TEMPLATE, color_str);

  gtk_css_provider_load_from_data (priv->color_provider,
                                   parsed_css,
                                   -1,
                                   NULL);

  gdk_rgba_free (color);
  g_free (color_str);
}

static void
gtd_list_view__update_done_label (GtdListView *view)
{
  gchar *new_label;

  g_return_if_fail (GTD_IS_LIST_VIEW (view));

  new_label = g_strdup_printf ("%s (%d)",
                               _("Done"),
                               view->priv->complete_tasks);

  gtk_label_set_label (view->priv->done_label, new_label);

  g_free (new_label);
}

static gboolean
can_toggle_show_completed (GtdListView *view)
{
  view->priv->can_toggle = TRUE;
  return G_SOURCE_REMOVE;
}

static void
gtd_list_view__done_button_clicked (GtkButton *button,
                                    gpointer   user_data)
{
  GtdListView *view = GTD_LIST_VIEW (user_data);
  gboolean show_completed;

  g_return_if_fail (GTD_IS_LIST_VIEW (view));

  if (!view->priv->can_toggle)
    return;

  /*
   * The can_toggle bitfield is needed because the user
   * can click mindlessly the Done button, while the row
   * animations are not finished. While the animation is
   * running, we ignore other clicks.
   */
  view->priv->can_toggle = FALSE;

  show_completed = view->priv->show_completed;

  gtd_list_view_set_show_completed (view, !show_completed);

  g_timeout_add (205,
                 (GSourceFunc) can_toggle_show_completed,
                 user_data);
}

static gint
gtd_list_view__listbox_sort_func (GtdTaskRow *row1,
                                  GtdTaskRow *row2,
                                  gpointer    user_data)
{
  g_return_val_if_fail (GTD_IS_TASK_ROW (row1), 0);
  g_return_val_if_fail (GTD_IS_TASK_ROW (row2), 0);

  if (gtd_task_row_get_new_task_mode (row1))
    return 1;
  else if (gtd_task_row_get_new_task_mode (row2))
    return -1;
  else
    return gtd_task_compare (gtd_task_row_get_task (row1), gtd_task_row_get_task (row2));
}

static void
gtd_list_view__clear_list (GtdListView *view)
{
  GList *children;
  GList *l;

  g_return_if_fail (GTD_IS_LIST_VIEW (view));

  view->priv->complete_tasks = 0;
  gtd_arrow_frame_set_row (view->priv->arrow_frame, NULL);

  children = gtk_container_get_children (GTK_CONTAINER (view->priv->listbox));

  for (l = children; l != NULL; l = l->next)
    {
      if (l->data != view->priv->new_task_row)
        {
          g_signal_handlers_disconnect_by_func (gtd_task_row_get_task (l->data),
                                                gtd_list_view__task_completed,
                                                view);
          gtk_widget_destroy (l->data);
        }
    }

  gtk_revealer_set_reveal_child (view->priv->revealer, FALSE);
  gtk_revealer_set_reveal_child (view->priv->edit_revealer, FALSE);

  g_list_free (children);
}

static void
gtd_list_view__row_activated (GtkListBox *listbox,
                              GtdTaskRow *row,
                              gpointer    user_data)
{
  GtdListViewPrivate *priv = GTD_LIST_VIEW (user_data)->priv;

  if (row == priv->new_task_row)
    return;

  gtd_edit_pane_set_task (priv->edit_pane, gtd_task_row_get_task (row));

  gtk_revealer_set_reveal_child (priv->edit_revealer, TRUE);
  gtd_arrow_frame_set_row (priv->arrow_frame, row);
}

static void
gtd_list_view__add_task (GtdListView *view,
                         GtdTask     *task)
{
  GtdListViewPrivate *priv = view->priv;
  GtkWidget *new_row;

  g_return_if_fail (GTD_IS_LIST_VIEW (view));
  g_return_if_fail (GTD_IS_TASK (task));

  new_row = gtd_task_row_new (task);

  if (!gtd_task_get_complete (task))
    {
      gtk_list_box_insert (priv->listbox,
                           new_row,
                           0);
      gtd_task_row_reveal (GTD_TASK_ROW (new_row));
    }
  else
    {
      priv->complete_tasks++;

      gtd_list_view__update_done_label (view);

      if (!gtk_revealer_get_reveal_child (priv->revealer))
        gtk_revealer_set_reveal_child (priv->revealer, TRUE);
    }
}

static void
gtd_list_view__remove_task (GtdListView *view,
                            GtdTask     *task)
{
  GtdListViewPrivate *priv = view->priv;
  GList *children;
  GList *l;

  g_return_if_fail (GTD_IS_LIST_VIEW (view));
  g_return_if_fail (GTD_IS_TASK (task));

  children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

  for (l = children; l != NULL; l = l->next)
    {
      if (!gtd_task_row_get_new_task_mode (l->data) &&
          gtd_task_row_get_task (l->data) == task)
        {
          gtd_task_row_destroy (l->data);
        }
    }

  if (children)
    g_list_free (children);
}

static void
gtd_list_view__task_completed (GObject    *object,
                               GParamSpec *spec,
                               gpointer    user_data)
{
  GtdListViewPrivate *priv = GTD_LIST_VIEW (user_data)->priv;
  GtdTask *task = GTD_TASK (object);
  gboolean task_complete;

  g_return_if_fail (GTD_IS_TASK (object));
  g_return_if_fail (GTD_IS_LIST_VIEW (user_data));

  task_complete = gtd_task_get_complete (task);

  gtd_manager_update_task (priv->manager, task);
  gtd_task_list_save_task (gtd_task_get_list (task), task);

  if (task_complete)
    priv->complete_tasks++;
  else
    priv->complete_tasks--;

  gtd_list_view__update_done_label (GTD_LIST_VIEW (user_data));
  gtk_revealer_set_reveal_child (priv->revealer, priv->complete_tasks > 0);

  if (!priv->show_completed)
    {
      if (task_complete)
        gtd_list_view__remove_task (GTD_LIST_VIEW (user_data), task);
      else
        gtd_list_view__add_task (GTD_LIST_VIEW (user_data), task);
    }
  else
    {
      gtk_list_box_invalidate_sort (priv->listbox);
    }
}

static void
gtd_list_view__task_added (GtdTaskList *list,
                           GtdTask     *task,
                           gpointer     user_data)
{
  GtdListViewPrivate *priv = GTD_LIST_VIEW (user_data)->priv;

  g_return_if_fail (GTD_IS_LIST_VIEW (user_data));
  g_return_if_fail (GTD_IS_TASK_LIST (list));
  g_return_if_fail (GTD_IS_TASK (task));

  /* Add the new task to the list */
  gtd_list_view__add_task (GTD_LIST_VIEW (user_data), task);
}

static void
gtd_list_view__create_task (GtdTaskRow *row,
                            GtdTask    *task,
                            gpointer    user_data)
{
  GtdListViewPrivate *priv = GTD_LIST_VIEW (user_data)->priv;
  GtkWidget *new_row;

  g_return_if_fail (GTD_IS_LIST_VIEW (user_data));
  g_return_if_fail (GTD_IS_TASK_ROW (row));
  g_return_if_fail (GTD_IS_TASK (task));
  g_return_if_fail (!priv->readonly);
  g_return_if_fail (priv->task_list);

  /*
   * Newly created tasks are not aware of
   * their parent lists.
   */
  gtd_task_set_list (task, priv->task_list);
  gtd_task_list_save_task (priv->task_list, task);

  gtd_manager_create_task (priv->manager, task);
}

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

    case PROP_SHOW_COMPLETED:
      g_value_set_boolean (value, self->priv->show_completed);
      break;

    case PROP_SHOW_LIST_NAME:
      g_value_set_boolean (value, self->priv->show_list_name);
      break;

    case PROP_READONLY:
      g_value_set_boolean (value, self->priv->readonly);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_list_view_constructed (GObject *object)
{
  GtdListView *self = GTD_LIST_VIEW (object);

  G_OBJECT_CLASS (gtd_list_view_parent_class)->constructed (object);

  /* css provider */
  self->priv->color_provider = gtk_css_provider_new ();

  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self->priv->viewport)),
                                  GTK_STYLE_PROVIDER (self->priv->color_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2);

  /* show a nifty separator between lines */
  gtk_list_box_set_sort_func (self->priv->listbox,
                              (GtkListBoxSortFunc) gtd_list_view__listbox_sort_func,
                              NULL,
                              NULL);
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
      gtd_list_view_set_manager (self, g_value_get_object (value));
      break;

    case PROP_SHOW_COMPLETED:
      gtd_list_view_set_show_completed (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LIST_NAME:
      gtd_list_view_set_show_list_name (self, g_value_get_boolean (value));
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
  object_class->constructed = gtd_list_view_constructed;
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
                              TRUE,
                              G_PARAM_READWRITE));

  /**
   * GtdListView::show-list-name:
   *
   * Whether the task rows should show the list name.
   */
  g_object_class_install_property (
        object_class,
        PROP_SHOW_LIST_NAME,
        g_param_spec_boolean ("show-list-name",
                              _("Whether task rows show the list name"),
                              _("Whether task rows show the list name at the end of the row"),
                              FALSE,
                              G_PARAM_READWRITE));

  /**
   * GtdListView::show-completed:
   *
   * Whether completed tasks are shown.
   */
  g_object_class_install_property (
        object_class,
        PROP_SHOW_COMPLETED,
        g_param_spec_boolean ("show-completed",
                              _("Whether completed tasks are shown"),
                              _("Whether completed tasks are visible or not"),
                              FALSE,
                              G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/list-view.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, arrow_frame);
  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, edit_pane);
  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, edit_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, done_image);
  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, done_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtdListView, viewport);

  gtk_widget_class_bind_template_callback (widget_class, gtd_list_view__done_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, gtd_list_view__edit_task_finished);
  gtk_widget_class_bind_template_callback (widget_class, gtd_list_view__remove_task_cb);
  gtk_widget_class_bind_template_callback (widget_class, gtd_list_view__row_activated);
}

static void
gtd_list_view_init (GtdListView *self)
{
  self->priv = gtd_list_view_get_instance_private (self);
  self->priv->readonly = TRUE;
  self->priv->can_toggle = TRUE;
  self->priv->new_task_row = GTD_TASK_ROW (gtd_task_row_new (NULL));
  gtd_task_row_set_new_task_mode (self->priv->new_task_row, TRUE);

  g_signal_connect (self->priv->new_task_row,
                    "create-task",
                    G_CALLBACK (gtd_list_view__create_task),
                    self);

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
      gtd_edit_pane_set_manager (GTD_EDIT_PANE (view->priv->edit_pane), manager);

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
  GtdListViewPrivate *priv = view->priv;

  g_return_if_fail (GTD_IS_LIST_VIEW (view));
  g_return_if_fail (GTD_IS_TASK_LIST (list));

  if (priv->task_list != list)
    {
      GdkRGBA *color;
      gchar *color_str;
      gchar *parsed_css;
      GList *task_list;
      GList *l;

      /*
       * Disconnect the old GtdTaskList signals.
       */
      if (priv->task_list)
        {
          g_signal_handlers_disconnect_by_func (priv->task_list,
                                                gtd_list_view__task_added,
                                                view);
          g_signal_handlers_disconnect_by_func (priv->task_list,
                                                gtd_list_view__color_changed,
                                                view);
        }

      /* Add the color to provider */
      color = gtd_task_list_get_color (list);
      color_str = gdk_rgba_to_string (color);

      parsed_css = g_strdup_printf (COLOR_TEMPLATE, color_str);

      g_debug ("setting style for provider: %s", parsed_css);

      gtk_css_provider_load_from_data (priv->color_provider,
                                       parsed_css,
                                       -1,
                                       NULL);

      gdk_rgba_free (color);
      g_free (color_str);

      /* Load taska */
      priv->task_list = list;

      /* clear previous tasks */
      gtd_list_view__clear_list (view);

      /* Add the tasks from the list */
      task_list = gtd_task_list_get_tasks (list);

      for (l = task_list; l != NULL; l = l->next)
        {
          gtd_list_view__add_task (view, l->data);

          g_signal_connect (l->data,
                            "notify::complete",
                            G_CALLBACK (gtd_list_view__task_completed),
                            view);
        }

      g_list_free (task_list);

      g_signal_connect (list,
                        "task-added",
                        G_CALLBACK (gtd_list_view__task_added),
                        view);
      g_signal_connect_swapped (list,
                                "task-removed",
                                G_CALLBACK (gtd_list_view__remove_task),
                                view);
      g_signal_connect (list,
                        "notify::color",
                        G_CALLBACK (gtd_list_view__color_changed),
                        view);
    }
}

/**
 * gtd_list_view_get_show_list_name:
 * @view: a #GtdListView
 *
 * Whether @view shows the tasks' list names.
 *
 * Returns: %TRUE if @view show the tasks' list names, %FALSE otherwise
 */
gboolean
gtd_list_view_get_show_list_name (GtdListView *view)
{
  g_return_val_if_fail (GTD_IS_LIST_VIEW (view), FALSE);

  return view->priv->show_list_name;
}

/**
 * gtd_list_view_set_show_list_name:
 * @view: a #GtdListView
 * @show_list_name: %TRUE to show list names, %FALSE to hide it
 *
 * Whether @view should should it's tasks' list name.
 *
 * Returns:
 */
void
gtd_list_view_set_show_list_name (GtdListView *view,
                                  gboolean     show_list_name)
{
  g_return_if_fail (GTD_IS_LIST_VIEW (view));

  if (view->priv->show_list_name != show_list_name)
    {
      GList *children;
      GList *l;

      view->priv->show_list_name = show_list_name;

      /* update current children */
      children = gtk_container_get_children (GTK_CONTAINER (view->priv->listbox));

      for (l = children; l != NULL; l = l->next)
        gtd_task_row_set_list_name_visible (l->data, show_list_name);

      g_list_free (children);

      g_object_notify (G_OBJECT (view), "show-list-name");
    }
}

/**
 * gtd_list_view_get)show_completed:
 * @view: a #GtdListView
 *
 * Returns %TRUE if completed tasks are visible, %FALSE otherwise.
 *
 * Returns: %TRUE if completed tasks are visible, %FALSE if they are hidden
 */
gboolean
gtd_list_view_get_show_completed (GtdListView *view)
{
  g_return_val_if_fail (GTD_IS_LIST_VIEW (view), FALSE);

  return view->priv->show_completed;
}

/**
 * gtd_list_view_set_show_completed:
 * @view: a #GtdListView
 * @show_completed: %TRUE to show completed tasks, %FALSE to hide them
 *
 * Sets the ::show-completed property to @show_completed.
 *
 * Returns:
 */
void
gtd_list_view_set_show_completed (GtdListView *view,
                                  gboolean     show_completed)
{
  GtdListViewPrivate *priv = view->priv;

  g_return_if_fail (GTD_IS_LIST_VIEW (view));

  if (priv->show_completed != show_completed)
    {

      priv->show_completed = show_completed;

      gtk_image_set_from_icon_name (view->priv->done_image,
                                    show_completed ? "zoom-out-symbolic" : "zoom-in-symbolic",
                                    GTK_ICON_SIZE_BUTTON);


      /* insert or remove list rows */
      if (show_completed)
        {
          GList *list_of_tasks;
          GList *l;

          list_of_tasks = gtd_list_view_get_list (view);

          for (l = list_of_tasks; l != NULL; l = l->next)
            {
              GtkWidget *new_row;

              if (!gtd_task_get_complete (l->data))
                continue;

              new_row = gtd_task_row_new (l->data);

              gtk_list_box_insert (priv->listbox,
                                   new_row,
                                   0);

              gtd_task_row_reveal (GTD_TASK_ROW (new_row));
            }

            if (list_of_tasks)
              g_list_free (list_of_tasks);
        }
      else
        {
          GList *children;
          GList *l;

          children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

          for (l = children; l != NULL; l = l->next)
            {
              if (!gtd_task_row_get_new_task_mode (l->data) &&
                  gtd_task_get_complete (gtd_task_row_get_task (l->data)))
                {
                  gtd_task_row_destroy (l->data);
                }
            }

          if (children)
              g_list_free (children);
        }

      g_object_notify (G_OBJECT (view), "show-completed");
    }
}
