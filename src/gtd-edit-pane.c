/* gtd-edit-pane.c
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

#include "gtd-edit-pane.h"
#include "gtd-manager.h"
#include "gtd-task.h"
#include "gtd-task-list.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkCalendar       *calendar;
  GtkLabel          *date_label;
  GtkTextView       *notes_textview;
  GtkComboBoxText   *priority_combo;

  /* task bindings */
  GBinding          *notes_binding;
  GBinding          *priority_binding;


  GtdManager        *manager;
  GtdTask           *task;
} GtdEditPanePrivate;

struct _GtdEditPane
{
  GtkGrid             parent;

  /*<private>*/
  GtdEditPanePrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdEditPane, gtd_edit_pane, GTK_TYPE_GRID)

enum {
  PROP_0,
  PROP_MANAGER,
  PROP_TASK,
  LAST_PROP
};

enum {
  EDIT_FINISHED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void             gtd_edit_pane__date_selected              (GtkCalendar      *calendar,
                                                                   gpointer          user_data);

static void
gtd_edit_pane__close_button_clicked (GtkButton *button,
                                     gpointer   user_data)
{
  GtdEditPanePrivate *priv;

  g_return_if_fail (GTD_IS_EDIT_PANE (user_data));

  priv = GTD_EDIT_PANE (user_data)->priv;

  /* save the task */
  gtd_task_save (priv->task);

  g_signal_emit (user_data, signals[EDIT_FINISHED], 0, priv->task);
}

static void
gtd_edit_pane_update_date (GtdEditPane *pane)
{
  GtdEditPanePrivate *priv;
  GDateTime *dt;
  gchar *text;

  g_return_if_fail (GTD_IS_EDIT_PANE (pane));

  priv = pane->priv;
  dt = priv->task ? gtd_task_get_due_date (priv->task) : NULL;
  text = dt ? g_date_time_format (dt, "%x") : NULL;

  if (dt)
    {
      g_signal_handlers_block_by_func (priv->calendar,
                                       gtd_edit_pane__date_selected,
                                       pane);

      gtk_calendar_select_month (priv->calendar,
                                 g_date_time_get_month (dt) - 1,
                                 g_date_time_get_year (dt));
      gtk_calendar_select_day (priv->calendar,
                               g_date_time_get_day_of_month (dt));
      gtk_calendar_mark_day (priv->calendar,
                             g_date_time_get_day_of_month (dt));

      g_signal_handlers_unblock_by_func (priv->calendar,
                                         gtd_edit_pane__date_selected,
                                         pane);
    }

  gtk_label_set_label (priv->date_label, text ? text : _("No date set"));

  g_free (text);
}

static void
gtd_edit_pane__date_selected (GtkCalendar *calendar,
                              gpointer     user_data)
{
  GtdEditPanePrivate *priv;
  GDateTime *new_dt;
  gchar *text;
  guint year;
  guint month;
  guint day;

  g_return_if_fail (GTD_IS_EDIT_PANE (user_data));

  priv = GTD_EDIT_PANE (user_data)->priv;

  gtk_calendar_get_date (calendar,
                         &year,
                         &month,
                         &day);

  new_dt = g_date_time_new_local (year,
                                  month + 1,
                                  day,
                                  0,
                                  0,
                                  0);

  text = g_date_time_format (new_dt, "%x");

  gtd_task_set_due_date (priv->task, new_dt);
  gtk_label_set_label (priv->date_label, text);

  g_date_time_unref (new_dt);
  g_free (text);
}

static void
gtd_edit_pane_finalize (GObject *object)
{
  GtdEditPane *self = (GtdEditPane *)object;
  GtdEditPanePrivate *priv = gtd_edit_pane_get_instance_private (self);

  G_OBJECT_CLASS (gtd_edit_pane_parent_class)->finalize (object);
}

static void
gtd_edit_pane_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtdEditPane *self = GTD_EDIT_PANE (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->priv->manager);
      break;

    case PROP_TASK:
      g_value_set_object (value, self->priv->task);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_edit_pane_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtdEditPane *self = GTD_EDIT_PANE (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->priv->manager = g_value_get_object (value);
      break;

    case PROP_TASK:
      self->priv->task = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_edit_pane_class_init (GtdEditPaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_edit_pane_finalize;
  object_class->get_property = gtd_edit_pane_get_property;
  object_class->set_property = gtd_edit_pane_set_property;

  /**
   * GtdEditPane::manager:
   *
   * A weak reference to the application's #GtdManager instance.
   */
  g_object_class_install_property (
        object_class,
        PROP_MANAGER,
        g_param_spec_object ("manager",
                             _("Manager of this application"),
                             _("The manager of the application"),
                             GTD_TYPE_MANAGER,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GtdEditPane::task:
   *
   * The task that is actually being edited.
   */
  g_object_class_install_property (
        object_class,
        PROP_TASK,
        g_param_spec_object ("task",
                             _("Task being edited"),
                             _("The task that is actually being edited"),
                             GTD_TYPE_TASK,
                             G_PARAM_READWRITE));

  /**
   * GtdEditPane::edit-finished:
   *
   * Emitted when the the user finishes editing the task, i.e. the pane is closed.
   */
  signals[EDIT_FINISHED] = g_signal_new ("edit-finished",
                                         GTD_TYPE_EDIT_PANE,
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL,
                                         NULL,
                                         NULL,
                                         G_TYPE_NONE,
                                         1,
                                         GTD_TYPE_TASK);

  /* template class */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/edit-pane.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdEditPane, calendar);
  gtk_widget_class_bind_template_child_private (widget_class, GtdEditPane, date_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtdEditPane, notes_textview);
  gtk_widget_class_bind_template_child_private (widget_class, GtdEditPane, priority_combo);

  gtk_widget_class_bind_template_callback (widget_class, gtd_edit_pane__close_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, gtd_edit_pane__date_selected);
}

static void
gtd_edit_pane_init (GtdEditPane *self)
{
  self->priv = gtd_edit_pane_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gtd_edit_pane_new (void)
{
  return g_object_new (GTD_TYPE_EDIT_PANE, NULL);
}

/**
 * gtd_edit_pane_get_manager:
 *
 * Retrieves the #GtdManager of the application.
 *
 * Returns: (transfer none): the #GtdManager of @pane
 */
GtdManager*
gtd_edit_pane_get_manager (GtdEditPane *pane)
{
  g_return_val_if_fail (GTD_IS_EDIT_PANE (pane), NULL);

  return pane->priv->manager;
}

/**
 * gtd_edit_pane_set_manager:
 * @pane: a #GtdEditPane
 * @manager: the singleton #GtdManager
 *
 * Sets the #GtdManager of the application.
 *
 * Returns:
 */
void
gtd_edit_pane_set_manager (GtdEditPane *pane,
                           GtdManager  *manager)
{
  g_return_if_fail (GTD_IS_EDIT_PANE (pane));
  g_return_if_fail (GTD_IS_MANAGER (manager));

  if (pane->priv->manager != manager)
    {
      pane->priv->manager = manager;

      g_object_notify (G_OBJECT (pane), "manager");
    }
}

/**
 * gtd_edit_pane_get_task:
 * @pane: a #GtdEditPane
 *
 * Retrieves the currently edited #GtdTask of %pane, or %NULL if none is set.
 *
 * Returns: (transfer none): the current #GtdTask being edited from %pane.
 */
GtdTask*
gtd_edit_pane_get_task (GtdEditPane *pane)
{
  g_return_val_if_fail (GTD_IS_EDIT_PANE (pane), NULL);

  return pane->priv->task;
}

/**
 * gtd_edit_pane_set_task:
 * @pane: a #GtdEditPane
 * @task: a #GtdTask or %NULL
 *
 * Sets %task as the currently editing task of %pane.
 *
 * Returns:
 */
void
gtd_edit_pane_set_task (GtdEditPane *pane,
                        GtdTask     *task)
{
  GtdEditPanePrivate *priv;

  g_return_if_fail (GTD_IS_EDIT_PANE (pane));

  priv = pane->priv;

  if (priv->task != task)
    {
      if (priv->task)
        {
          /* Save the old task before exiting */
          gtd_task_save (priv->task);

          gtd_manager_update_task (priv->manager, priv->task);
          gtd_task_list_save_task (gtd_task_get_list (priv->task), task);

          g_clear_pointer (&priv->notes_binding, g_binding_unbind);
          g_clear_pointer (&priv->priority_binding, g_binding_unbind);
        }

      priv->task = task;

      if (task)
        {
          /* due date */
          gtd_edit_pane_update_date (pane);

          /* description */
          gtk_text_buffer_set_text (gtk_text_view_get_buffer (priv->notes_textview),
                                    gtd_task_get_description (task),
                                    -1);
          priv->notes_binding = g_object_bind_property (task,
                                                        "description",
                                                        gtk_text_view_get_buffer (priv->notes_textview),
                                                        "text",
                                                        G_BINDING_BIDIRECTIONAL);

          /* priority */
          gtk_combo_box_set_active (GTK_COMBO_BOX (priv->priority_combo), CLAMP (gtd_task_get_priority (task),
                                                                                 0,
                                                                                 3));
          priv->priority_binding = g_object_bind_property (task,
                                                           "priority",
                                                           priv->priority_combo,
                                                           "active",
                                                           G_BINDING_BIDIRECTIONAL);
        }

      g_object_notify (G_OBJECT (pane), "task");
    }
}
