/* gtd-task-list.h
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

#ifndef GTD_TASK_LIST_H
#define GTD_TASK_LIST_H

#include <glib-object.h>

#include "gtd-object.h"

#include <libecal/libecal.h>

G_BEGIN_DECLS

#define GTD_TYPE_TASK_LIST (gtd_task_list_get_type())

G_DECLARE_FINAL_TYPE (GtdTaskList, gtd_task_list, GTD, TASK_LIST, GtdObject)

GtdTaskList*            gtd_task_list_new                       (ESource                *source);

const gchar*            gtd_task_list_get_name                  (GtdTaskList            *list);

void                    gtd_task_list_set_name                  (GtdTaskList            *list,
                                                                 const gchar            *name);

GList*                  gtd_task_list_get_tasks                 (GtdTaskList            *list);

void                    gtd_task_list_save_task                 (GtdTaskList            *list,
                                                                 GtdTask                *task);

void                    gtd_task_list_remove_task               (GtdTaskList            *list,
                                                                 GtdTask                *task);

gboolean                gtd_task_list_contains                  (GtdTaskList            *list,
                                                                 GtdTask                *task);

ESource*                gtd_task_list_get_source                (GtdTaskList            *list);

G_END_DECLS

#endif /* GTD_TASK_LIST_H */
