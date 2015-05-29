/* gtd-window.c
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
#include "gtd-list-view.h"
#include "gtd-manager.h"
#include "gtd-task-list.h"
#include "gtd-task-list-item.h"
#include "gtd-window.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkButton                     *back_button;
  GtkColorButton                *color_button;
  GtkHeaderBar                  *headerbar;
  GtkFlowBox                    *lists_flowbox;
  GtkStack                      *main_stack;
  GtkButton                     *notification_action_button;
  GtkLabel                      *notification_label;
  GtkRevealer                   *notification_revealer;
  GtkSpinner                    *notification_spinner;
  GtkStackSwitcher              *stack_switcher;
  GtdListView                   *list_view;

  /* mode */
  GtdWindowMode                  mode;

  /*
   * A queue of the next operations.
   */
  GQueue                        *notification_queue;
  gint                           notification_delay_id;
  gboolean                       consuming_notifications;

  GtdManager                    *manager;
} GtdWindowPrivate;

struct _GtdWindow
{
  GtkApplicationWindow  application;

  /*< private >*/
  GtdWindowPrivate     *priv;
};

typedef struct
{
  GtdWindow   *window;

  gchar       *id;
  gchar       *text;
  gchar       *label;

  gint         delay;
  gboolean     show_spinner;

  GSourceFunc  primary_action;
  GSourceFunc  secondary_action;

  gpointer     data;
} NotificationData;

#define LOADING_LISTS_NOTIFICATION_ID            "loading-lists-id"

static gboolean      gtd_window__execute_notification_data       (NotificationData      *data);


static void          gtd_window_consume_notification             (GtdWindow             *window);

void                 notification_data_free                      (NotificationData      *data);

G_DEFINE_TYPE_WITH_PRIVATE (GtdWindow, gtd_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_MANAGER,
  LAST_PROP
};

void
notification_data_free (NotificationData *data)
{
  g_free (data->id);
  g_free (data->text);
  g_free (data->label);
  g_free (data);
}

static void
gtd_window__notification_close_button_clicked (GtkButton *button,
                                               gpointer   user_data)
{
  GtdWindowPrivate *priv;
  NotificationData *data;

  g_return_if_fail (GTD_IS_WINDOW (user_data));

  priv = GTD_WINDOW (user_data)->priv;
  data = g_queue_pop_head (priv->notification_queue);

  if (data->primary_action)
    data->primary_action (data->data);

  gtd_window_consume_notification (GTD_WINDOW (user_data));
}

static void
gtd_window_consume_notification (GtdWindow *window)
{
  GtdWindowPrivate *priv;
  NotificationData *data;

  g_return_if_fail (GTD_IS_WINDOW (window));

  priv = window->priv;

  if (g_queue_is_empty (priv->notification_queue))
    {
      gtk_revealer_set_reveal_child (priv->notification_revealer, FALSE);
      priv->consuming_notifications = FALSE;
      return;
    }

  priv->consuming_notifications = TRUE;

  /* Keep the current operation until it is finished or canceled */
  data = g_queue_peek_head (priv->notification_queue);

  gtk_button_set_label (priv->notification_action_button, data->label ? data->label : "");
  gtk_label_set_markup (priv->notification_label, data->text);
  gtk_revealer_set_reveal_child (priv->notification_revealer, TRUE);
  gtk_widget_set_visible (GTK_WIDGET (priv->notification_action_button), data->label != NULL);

  /* spinner */
  gtk_widget_set_visible (GTK_WIDGET (priv->notification_spinner), data->show_spinner);
  g_object_set (priv->notification_spinner, "active", data->show_spinner, NULL);

  /* If there's a delay set, execute the action */
  if (data->delay)
    {
      priv->notification_delay_id = g_timeout_add (data->delay,
                                                   (GSourceFunc) gtd_window__execute_notification_data,
                                                   data);
    }
}

static gboolean
gtd_window__execute_notification_data (NotificationData *data)
{
  GtdWindowPrivate *priv = data->window->priv;
  gboolean retval = G_SOURCE_REMOVE;

  if (data->primary_action)
    retval = data->primary_action (data->data);

  notification_data_free (data);

  priv->notification_delay_id = 0;

  return retval;
}

static void
gtd_window__list_color_set (GtkColorChooser *button,
                            gpointer         user_data)
{
  GtdWindowPrivate *priv = GTD_WINDOW (user_data)->priv;
  GtdTaskList *list;
  GdkRGBA new_color;

  g_return_if_fail (GTD_IS_WINDOW (user_data));
  g_return_if_fail (gtd_list_view_get_task_list (priv->list_view));

  list = gtd_list_view_get_task_list (priv->list_view);

  g_debug ("%s: %s: %s",
           G_STRFUNC,
           _("Setting new color for task list"),
           gtd_task_list_get_name (list));

  gtk_color_chooser_get_rgba (button, &new_color);
  gtd_task_list_set_color (list, &new_color);

  gtd_manager_save_task_list (priv->manager, list);
}

static gint
gtd_window__listbox_sort_func (GtdTaskListItem *a,
                               GtdTaskListItem *b,
                               gpointer         user_data)
{
  GtdTaskList *l1;
  GtdTaskList *l2;
  gint retval = 0;

  l1 = gtd_task_list_item_get_list (a);
  l2 = gtd_task_list_item_get_list (b);

  retval = g_strcmp0 (gtd_task_list_get_origin (l1), gtd_task_list_get_origin (l2));

  if (retval != 0)
    return retval;

  return g_strcmp0 (gtd_task_list_get_name (l1), gtd_task_list_get_name (l2));
}

static void
gtd_window__manager_ready_changed (GObject    *source,
                                   GParamSpec *spec,
                                   gpointer    user_data)
{
  GtdWindowPrivate *priv = GTD_WINDOW (user_data)->priv;
  g_return_if_fail (GTD_IS_WINDOW (user_data));

  if (gtd_object_get_ready (GTD_OBJECT (source)))
    {
      gtd_window_cancel_notification (GTD_WINDOW (user_data), LOADING_LISTS_NOTIFICATION_ID);
    }
  else
    {
      gtd_window_notify (GTD_WINDOW (user_data),
                         0,
                         LOADING_LISTS_NOTIFICATION_ID,
                         _("Loading your task lists…"),
                         NULL,
                         NULL,
                         NULL,
                         TRUE,
                         NULL);
    }
}

static void
gtd_window__back_button_clicked (GtkButton *button,
                                 gpointer   user_data)
{
  GtdWindowPrivate *priv = GTD_WINDOW (user_data)->priv;

  g_return_if_fail (GTD_IS_WINDOW (user_data));

  gtk_stack_set_visible_child_name (priv->main_stack, "overview");
  gtk_header_bar_set_custom_title (priv->headerbar, GTK_WIDGET (priv->stack_switcher));
  gtk_widget_hide (GTK_WIDGET (priv->back_button));
  gtk_widget_hide (GTK_WIDGET (priv->color_button));
}

static void
gtd_window__list_selected (GtkFlowBox      *flowbox,
                           GtdTaskListItem *item,
                           gpointer         user_data)
{
  GtdWindowPrivate *priv = GTD_WINDOW (user_data)->priv;
  GtdTaskList *list;
  GdkRGBA *list_color;

  g_return_if_fail (GTD_IS_WINDOW (user_data));
  g_return_if_fail (GTD_IS_TASK_LIST_ITEM (item));

  list = gtd_task_list_item_get_list (item);
  list_color = gtd_task_list_get_color (list);

  g_signal_handlers_block_by_func (priv->color_button,
                                   gtd_window__list_color_set,
                                   user_data);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (priv->color_button), list_color);

  gtk_stack_set_visible_child_name (priv->main_stack, "tasks");
  gtk_header_bar_set_title (priv->headerbar, gtd_task_list_get_name (list));
  gtk_header_bar_set_subtitle (priv->headerbar, gtd_task_list_get_origin (list));
  gtk_header_bar_set_custom_title (priv->headerbar, NULL);
  gtd_list_view_set_task_list (priv->list_view, list);
  gtd_list_view_set_show_completed (priv->list_view, FALSE);
  gtk_widget_show (GTK_WIDGET (priv->back_button));
  gtk_widget_show (GTK_WIDGET (priv->color_button));

  g_signal_handlers_unblock_by_func (priv->color_button,
                                     gtd_window__list_color_set,
                                     user_data);

  gdk_rgba_free (list_color);
}

static void
gtd_window__list_added (GtdManager  *manager,
                        GtdTaskList *list,
                        gpointer     user_data)
{
  GtdWindowPrivate *priv = GTD_WINDOW (user_data)->priv;
  GtkWidget *item;

  item = gtd_task_list_item_new (list);
  gtk_widget_show (item);

  gtk_flow_box_insert (priv->lists_flowbox,
                       item,
                       -1);
}

static void
gtd_window_constructed (GObject *object)
{
  GtdWindowPrivate *priv = GTD_WINDOW (object)->priv;

  G_OBJECT_CLASS (gtd_window_parent_class)->constructed (object);

  gtk_flow_box_set_sort_func (priv->lists_flowbox,
                              (GtkFlowBoxSortFunc) gtd_window__listbox_sort_func,
                              NULL,
                              NULL);
}

GtkWidget*
gtd_window_new (GtdApplication *application)
{
  return g_object_new (GTD_TYPE_WINDOW,
                       "application", application,
                       "manager",     gtd_application_get_manager (application),
                       NULL);
}

static void
gtd_window_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtd_window_parent_class)->finalize (object);
}

static void
gtd_window_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtdWindow *self = GTD_WINDOW (object);

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
gtd_window_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtdWindow *self = GTD_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->manager = g_value_get_object (value);

      gtd_list_view_set_manager (self->priv->list_view, self->priv->manager);

      g_signal_connect (self->priv->manager,
                        "notify::ready",
                        G_CALLBACK (gtd_window__manager_ready_changed),
                        self);
      g_signal_connect (self->priv->manager,
                        "list-added",
                        G_CALLBACK (gtd_window__list_added),
                        self);

      g_object_notify (object, "manager");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_window_class_init (GtdWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_window_finalize;
  object_class->constructed = gtd_window_constructed;
  object_class->get_property = gtd_window_get_property;
  object_class->set_property = gtd_window_set_property;

  /**
   * GtdWindow::manager:
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
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/window.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, back_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, color_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, headerbar);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, lists_flowbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, list_view);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, main_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, notification_action_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, notification_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, notification_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, notification_spinner);
  gtk_widget_class_bind_template_child_private (widget_class, GtdWindow, stack_switcher);

  gtk_widget_class_bind_template_callback (widget_class, gtd_window__back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, gtd_window__list_color_set);
  gtk_widget_class_bind_template_callback (widget_class, gtd_window__list_selected);
  gtk_widget_class_bind_template_callback (widget_class, gtd_window__notification_close_button_clicked);
}

static void
gtd_window_init (GtdWindow *self)
{
  self->priv = gtd_window_get_instance_private (self);

  self->priv->notification_queue = g_queue_new ();

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gtd_window_get_manager:
 * @window: a #GtdWindow
 *
 * Retrieves a weak reference for the @window's #GtdManager.
 *
 * Returns: (transfer none): the #GtdManager of @window.
 */
GtdManager*
gtd_window_get_manager (GtdWindow *window)
{
  g_return_val_if_fail (GTD_IS_WINDOW (window), NULL);

  return window->priv->manager;
}

/**
 * gtd_window_notify:
 * @window: a #GtdWindow
 * @visible_time: time which the notification will be visible, 0 to show permanently
 * @text: text of the notification
 * @button_label: text to show on the notification button, %NULL to hide it
 * @primary_action: function to call when close button is clicked
 * @secondary_action: function to call when the alternative button is clicked
 * @show_spinner: whether show the spinner or not
 * @user_data: custom data
 *
 * Shows a notification on the top of the main window.
 *
 * Returns:
 */
void
gtd_window_notify (GtdWindow   *window,
                   gint         visible_time,
                   const gchar *id,
                   const gchar *text,
                   const gchar *button_label,
                   GSourceFunc  primary_action,
                   GSourceFunc  secondary_action,
                   gboolean     show_spinner,
                   gpointer     user_data)
{
  GtdWindowPrivate *priv;
  NotificationData *data;

  g_return_if_fail (GTD_IS_WINDOW (window));

  priv = window->priv;

  data = g_new0 (NotificationData, 1);
  data->window = window;
  data->delay = visible_time;
  data->id = g_strdup (id);
  data->text = g_strdup (text);
  data->label = g_strdup (button_label);
  data->primary_action = primary_action;
  data->secondary_action = secondary_action;
  data->show_spinner = show_spinner;
  data->data = user_data;

  g_queue_push_tail (priv->notification_queue, data);

  /* If we're not consuming notifications, start it now */
  if (!priv->consuming_notifications)
    gtd_window_consume_notification (window);
}

/**
 * gtd_window_cancel_notification:
 * @window: a #GtdManager
 * @id: id of the given notification
 *
 * Cancels the notification with @id as id.
 *
 * Returns:
 */
void
gtd_window_cancel_notification (GtdWindow   *window,
                                const gchar *id)
{
  GtdWindowPrivate *priv;
  NotificationData *data;
  GList *head;
  GList *l;
  gint index;

  g_return_if_fail (GTD_IS_WINDOW (window));
  g_return_if_fail (id != NULL);

  data = NULL;
  priv = window->priv;
  index = 0;

  /* Search for a notification with the given id */
  head = priv->notification_queue->head;

  for (l = head; l != NULL; l = l->next)
    {
      NotificationData *tmp = l->data;

      if (tmp && g_strcmp0 (tmp->id, id) == 0)
        {
          data = l->data;
          break;
        }

      index++;
    }

  g_queue_remove (priv->notification_queue, data);

  /* If we're removing the current head, continue consuming the queue */
  if (index == 0)
    {
      /* Remove any remaining timeouts */
      if (priv->notification_delay_id > 0)
        {
          g_source_remove (priv->notification_delay_id);
          priv->notification_delay_id = 0;
        }

      gtd_window_consume_notification (window);
    }

  if (data)
    notification_data_free (data);
}
