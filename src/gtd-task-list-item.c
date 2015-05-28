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

#include "gtd-task.h"
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

#define LUMINANCE(c)              (0.299 * c->red + 0.587 * c->green + 0.114 * c->blue)

#define THUMBNAIL_SIZE            192

enum {
  PROP_0,
  PROP_MODE,
  PROP_TASK_LIST,
  LAST_PROP
};

GdkPixbuf*
gtd_task_list_item__render_thumbnail (GtdTaskListItem *item)
{
  PangoFontDescription *font_desc;
  GtkStyleContext *context;
  cairo_surface_t *surface;
  GtkStateFlags state;
  PangoLayout *layout;
  GtdTaskList *list;
  GdkPixbuf *pix = NULL;
  GdkPixbuf *thumbnail;
  GtkBorder margin;
  GtkBorder padding;
  GdkRGBA *color;
  cairo_t *cr;
  GError *error = NULL;
  GList *tasks;

  /* TODO: review size here, maybe not hardcoded */
  list = item->priv->list;
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        THUMBNAIL_SIZE,
                                        THUMBNAIL_SIZE);
  cr = cairo_create (surface);

  /*
   * We'll draw the task names according to the font size, margin & padding
   * specified by the .thumbnail class. With that, it can be adapted to any
   * other themes.
   */
  context = gtk_widget_get_style_context (GTK_WIDGET (item));
  state = gtk_widget_get_state_flags (GTK_WIDGET (item));

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "thumbnail");

  /* Draw the thumbnail image */
  thumbnail = gdk_pixbuf_new_from_resource ("/org/gnome/todo/theme/bg.svg", &error);

  if (error)
    {
      g_warning ("Error loading thumbnail: %s", error->message);
      g_error_free (error);
      goto out;
    }

  gtk_render_icon (context,
                   cr,
                   thumbnail,
                   0.0,
                   0.0);

  /* Draw the list's background color */
  color = gtd_task_list_get_color (list);

  gdk_cairo_set_source_rgba (cr, color);

  cairo_rectangle (cr,
                   33.0,
                   9.0,
                   126.0,
                   174.0);

  cairo_fill (cr);

  /* Draw the first tasks from the list */
  gtk_style_context_get (context,
                         state,
                         "font", &font_desc,
                         NULL);
  gtk_style_context_get_margin (context,
                                state,
                                &margin);
  gtk_style_context_get_padding (context,
                                 state,
                                 &padding);

  layout = pango_cairo_create_layout (cr);
  tasks = gtd_task_list_get_tasks (list);

  /*
   * Sort the list, so that the first tasks are similar to what
   * the user will see when selecting the list.
   */
  tasks = g_list_sort (tasks, (GCompareFunc) gtd_task_compare);

  pango_layout_set_font_description (layout, font_desc);
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_width (layout, (126 - margin.left - margin.right) * PANGO_SCALE);

  /*
   * If the list exists and it's first element is a completed task,
   * we know for sure (since the list is already sorted) that there's
   * no undone tasks here.
   */
  if (!tasks ||
      (tasks && !gtd_task_get_complete (tasks->data)))
    {
      /* Draw the task name for each selected row. */
      gdouble x, y;
      GList *l;

      x = 33.0 + margin.left;
      y = 9.0 + margin.top;

      for (l = tasks; l != NULL; l = l->next)
        {
          gint font_height;

          /* Don't render completed tasks */
          if (gtd_task_get_complete (l->data))
            continue;

          y += padding.top;

          pango_layout_set_text (layout,
                                 gtd_task_get_title (l->data),
                                 -1);

          pango_layout_get_pixel_size (layout,
                                       NULL,
                                       &font_height);

          /*
           * If we reach the last visible row, it should draw a
           * "…" mark and stop drawing anything else
           */
          if (y + (padding.top + font_height + padding.bottom) + margin.bottom > 174)
            {
              pango_layout_set_text (layout,
                                     "…",
                                     -1);

              gtk_render_layout (context,
                             cr,
                             x,
                             y,
                             layout);
              break;
            }

          gtk_render_layout (context,
                             cr,
                             x,
                             y,
                             layout);

          y += font_height + padding.bottom;
        }

      g_list_free (tasks);
    }
  else
    {
      /*
       * If there's no task available, draw a "No tasks" string at
       * the middle of the list thumbnail.
       */
      gdouble y;
      gint font_height;

      pango_layout_set_text (layout,
                             _("No tasks"),
                             -1);
      pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
      pango_layout_get_pixel_size (layout,
                                   NULL,
                                   &font_height);

      y = (192 - font_height) / 2.0;

      gtk_render_layout (context,
                         cr,
                         33.0 + margin.left,
                         y,
                         layout);
    }

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  /* Retrieves the pixbuf from the drawed image */
  pix = gdk_pixbuf_get_from_surface (surface,
                                     0,
                                     0,
                                     THUMBNAIL_SIZE,
                                     THUMBNAIL_SIZE);

  gdk_rgba_free (color);

out:
  gtk_style_context_restore (context);
  cairo_surface_destroy (surface);
  cairo_destroy (cr);
  return pix;
}

static void
gtd_task_list_item__update_thumbnail (GtdTaskListItem *item)
{
  GtdTaskListItemPrivate *priv = item->priv;

  if (gtd_object_get_ready (GTD_OBJECT (priv->list)))
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->icon_image),
                                    "folder-documents-symbolic",
                                    GTK_ICON_SIZE_DIALOG);
    }
  else
    {
      GdkPixbuf *pix = gtd_task_list_item__render_thumbnail (item);

      gtk_image_set_from_pixbuf (GTK_IMAGE (priv->icon_image), pix);

      g_object_unref (pix);
    }
}

static void
gtd_task_list_item__task_changed (GtdTaskList *list,
                                  GtdTask     *task,
                                  gpointer     user_data)
{
  g_return_if_fail (GTD_IS_TASK_LIST_ITEM (user_data));

  gtd_task_list_item__update_thumbnail (GTD_TASK_LIST_ITEM (user_data));
}

static void
gtd_task_list_item__notify_ready (GtdTaskListItem *item,
                                  GParamSpec      *pspec,
                                  gpointer         user_data)
{
  gtd_task_list_item__update_thumbnail (item);
}

GtkWidget*
gtd_task_list_item_new (GtdTaskList *list)
{
  return g_object_new (GTD_TYPE_TASK_LIST_ITEM,
                       "task-list", list,
                       NULL);
}

static void
gtd_task_list_item_state_flags_changed (GtkWidget     *item,
                                         GtkStateFlags  flags)
{
  if (GTK_WIDGET_CLASS (gtd_task_list_item_parent_class)->state_flags_changed)
    GTK_WIDGET_CLASS (gtd_task_list_item_parent_class)->state_flags_changed (item, flags);

  gtd_task_list_item__update_thumbnail (GTD_TASK_LIST_ITEM (item));
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
      g_object_bind_property (priv->list,
                              "name",
                              priv->title_label,
                              "label",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      g_object_bind_property (priv->list,
                              "origin",
                              priv->subtitle_label,
                              "label",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      g_signal_connect_swapped (priv->list,
                                "notify::ready",
                                G_CALLBACK (gtd_task_list_item__notify_ready),
                                self);
      g_signal_connect (priv->list,
                       "task-added",
                        G_CALLBACK (gtd_task_list_item__task_changed),
                        self);
      g_signal_connect (priv->list,
                       "task-removed",
                        G_CALLBACK (gtd_task_list_item__task_changed),
                        self);
      g_signal_connect (priv->list,
                       "task-updated",
                        G_CALLBACK (gtd_task_list_item__task_changed),
                        self);

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

  widget_class->state_flags_changed = gtd_task_list_item_state_flags_changed;

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

/**
 * gtd_task_list_item_get_list:
 * @item: a #GtdTaskListItem
 *
 * Retrieves the internal #GtdTaskList from @item.
 *
 * Returns: (transfer none): the internal #GtdTaskList from @item
 */
GtdTaskList*
gtd_task_list_item_get_list (GtdTaskListItem *item)
{
  g_return_val_if_fail (GTD_IS_TASK_LIST_ITEM (item), NULL);

  return item->priv->list;
}
