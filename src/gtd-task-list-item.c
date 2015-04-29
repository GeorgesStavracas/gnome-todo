/* gtd-task-list-item.c
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

#include "gtd-task-list.h"
#include "gtd-task-list-item.h"

#include <glib/gi18n.h>

typedef struct
{
  GtkCheckButton            *selection_check;
  GtkImage                  *icon_image;
  GtkLabel                  *subtitle_label;
  GtkLabel                  *title_label;
  GtkSpinner                *spinner;

  /* data */
  GtdTaskList               *list;
  GtdWindowMode              mode;

} GtdTaskListItemPrivate;

struct _GtdTaskListItem
{
  GtkFlowBoxChild         parent;

  /*<private>*/
  GtdTaskListItemPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdTaskListItem, gtd_task_list_item, GTK_TYPE_FLOW_BOX_CHILD)

enum {
  PROP_0,
  PROP_MODE,
  PROP_TASK_LIST,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget*
gtd_task_list_item_new (GtdTaskList *list)
{
  return g_object_new (GTD_TYPE_TASK_LIST_ITEM,
                       "task-list", list,
                       NULL);
}

static void
gtd_task_list_item_finalize (GObject *object)
{
  GtdTaskListItem *self = (GtdTaskListItem *)object;
  GtdTaskListItemPrivate *priv = gtd_task_list_item_get_instance_private (self);

  G_OBJECT_CLASS (gtd_task_list_item_parent_class)->finalize (object);
}

static void
gtd_task_list_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtdTaskListItem *self = GTD_TASK_LIST_ITEM (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_int (value, self->priv->mode);
      break;

    case PROP_TASK_LIST:
      g_value_set_object (value, self->priv->list);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_task_list_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtdTaskListItem *self = GTD_TASK_LIST_ITEM (object);
  GtdTaskListItemPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_MODE:
      priv->mode = g_value_get_int (value);
      gtk_widget_set_visible (GTK_WIDGET (priv->selection_check),
                              priv->mode == GTD_WINDOW_MODE_SELECTION);
      break;

    case PROP_TASK_LIST:
      priv->list = g_value_get_object (value);
      gtk_label_set_label (GTK_LABEL (priv->title_label), gtd_task_list_get_name (priv->list));
      g_object_bind_property (priv->spinner,
                              "visible",
                              priv->list,
                              "ready",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_task_list_item_class_init (GtdTaskListItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_task_list_item_finalize;
  object_class->get_property = gtd_task_list_item_get_property;
  object_class->set_property = gtd_task_list_item_set_property;

  /**
   * GtdTaskListItem::mode:
   *
   * The parent source of the list.
   */
  g_object_class_install_property (
        object_class,
        PROP_MODE,
        g_param_spec_int ("mode",
                          _("Mode of this item"),
                          _("The mode of this item, inherited from the parent's mode"),
                          GTD_WINDOW_MODE_NORMAL,
                          GTD_WINDOW_MODE_SELECTION,
                          GTD_WINDOW_MODE_NORMAL,
                          G_PARAM_READWRITE));

  /**
   * GtdTaskListItem::task-list:
   *
   * The parent source of the list.
   */
  g_object_class_install_property (
        object_class,
        PROP_TASK_LIST,
        g_param_spec_object ("task-list",
                             _("Task list of the item"),
                             _("The task list associated with this item"),
                             GTD_TYPE_TASK_LIST,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /* template class */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/task-list-item.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtdTaskListItem, icon_image);
  gtk_widget_class_bind_template_child_private (widget_class, GtdTaskListItem, selection_check);
  gtk_widget_class_bind_template_child_private (widget_class, GtdTaskListItem, spinner);
  gtk_widget_class_bind_template_child_private (widget_class, GtdTaskListItem, subtitle_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtdTaskListItem, title_label);
}

static void
gtd_task_list_item_init (GtdTaskListItem *self)
{
  self->priv = gtd_task_list_item_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
