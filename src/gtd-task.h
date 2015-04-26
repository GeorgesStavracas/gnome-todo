/* gtd-task.h
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

#ifndef GTD_TASK_H
#define GTD_TASK_H

#include <glib-object.h>

#include "gtd-object.h"

G_BEGIN_DECLS

#define GTD_TYPE_TASK (gtd_task_get_type())

G_DECLARE_FINAL_TYPE (GtdTask, gtd_task, GTD, TASK, GtdObject)

struct _GtdTaskClass
{
  GtdObjectClass parent;
};

GtdTask*            gtd_task_new                      (const gchar          *uid);

gboolean            gtd_task_get_complete             (GtdTask              *task);

void                gtd_task_set_complete             (GtdTask              *task,
                                                       gboolean              complete);

const gchar*        gtd_task_get_description          (GtdTask              *task);

void                gtd_task_set_description          (GtdTask              *task,
                                                       const gchar          *description);

GDateTime*          gtd_task_get_due_date             (GtdTask              *task);

void                gtd_task_set_due_date             (GtdTask              *task,
                                                       GDateTime            *dt);

GtdTaskList*        gtd_task_get_list                 (GtdTask              *task);

void                gtd_task_set_list                 (GtdTask              *task,
                                                       GtdTaskList          *list);

gint                gtd_task_get_position             (GtdTask              *task);

void                gtd_task_set_position             (GtdTask              *task,
                                                       gint                  position);

const gchar*        gtd_task_get_title                (GtdTask              *task);

void                gtd_task_set_title                (GtdTask              *task,
                                                       const gchar          *title);

G_END_DECLS

#endif /* GTD_TASK_H */
