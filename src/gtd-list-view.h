/* gtd-list-view.h
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

#ifndef GTD_LIST_VIEW_H
#define GTD_LIST_VIEW_H

#include "gtd-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTD_TYPE_LIST_VIEW (gtd_list_view_get_type())

G_DECLARE_FINAL_TYPE (GtdListView, gtd_list_view, GTD, LIST_VIEW, GtkOverlay)

GtkWidget*                gtd_list_view_new                     (void);

GList*                    gtd_list_view_get_list                (GtdListView            *view);

void                      gtd_list_view_set_list                (GtdListView            *view,
                                                                 GList                  *list);

GtdManager*               gtd_list_view_get_manager             (GtdListView            *view);

void                      gtd_list_view_set_manager             (GtdListView            *view,
                                                                 GtdManager             *manager);

gboolean                  gtd_list_view_get_readonly            (GtdListView            *view);

void                      gtd_list_view_set_readonly            (GtdListView            *view,
                                                                 gboolean                readonly);

GtdTaskList*              gtd_list_view_get_task_list           (GtdListView            *view);

void                      gtd_list_view_set_task_list           (GtdListView            *view,
                                                                 GtdTaskList            *list);

gboolean                  gtd_list_view_get_show_list_name      (GtdListView            *view);

void                      gtd_list_view_set_show_list_name      (GtdListView            *view,
                                                                 gboolean                show_list_name);

gboolean                  gtd_list_view_get_show_completed      (GtdListView            *view);

void                      gtd_list_view_set_show_completed      (GtdListView            *view,
                                                                 gboolean                show_completed);
G_END_DECLS

#endif /* GTD_LIST_VIEW_H */
