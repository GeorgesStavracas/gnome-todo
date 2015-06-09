/* gtd-manager.h
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

#ifndef GTD_MANAGER_H
#define GTD_MANAGER_H

#include <glib-object.h>

#include "gtd-object.h"
#include "gtd-types.h"

#include <libecal/libecal.h>
#include <goa/goa.h>

G_BEGIN_DECLS

#define GTD_TYPE_MANAGER (gtd_manager_get_type())

G_DECLARE_FINAL_TYPE (GtdManager, gtd_manager, GTD, MANAGER, GtdObject)

GtdManager*             gtd_manager_new                   (void);

ESourceRegistry*        gtd_manager_get_source_registry   (GtdManager           *manager);

GList*                  gtd_manager_get_task_lists        (GtdManager           *manager);

void                    gtd_manager_remove_task_list      (GtdManager           *manager,
                                                           GtdTaskList          *list);

void                    gtd_manager_save_task_list        (GtdManager           *manager,
                                                           GtdTaskList          *list);

/* Tasks */
void                    gtd_manager_create_task           (GtdManager           *manager,
                                                           GtdTask              *task);

void                    gtd_manager_remove_task           (GtdManager           *manager,
                                                           GtdTask              *task);

void                    gtd_manager_update_task           (GtdManager           *manager,
                                                           GtdTask              *task);

/* Online accounts */
GoaClient*              gtd_manager_get_goa_client        (GtdManager           *manager);

gboolean                gtd_manager_is_goa_client_ready   (GtdManager           *manager);

G_END_DECLS

#endif /* GTD_MANAGER_H */
