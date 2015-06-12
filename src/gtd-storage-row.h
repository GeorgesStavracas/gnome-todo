/* gtd-storage-row.h
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

#ifndef GTD_GOA_ROW_H
#define GTD_GOA_ROW_H

#include "gtd-types.h"

#include <glib-object.h>
#include <goa/goa.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTD_TYPE_STORAGE_ROW (gtd_storage_row_get_type())

G_DECLARE_FINAL_TYPE (GtdStorageRow, gtd_storage_row, GTD, STORAGE_ROW, GtkListBoxRow)

GtkWidget*                  gtd_storage_row_new                  (GtdStorage          *storage);

gboolean                    gtd_storage_row_get_selected         (GtdStorageRow       *row);

void                        gtd_storage_row_set_selected         (GtdStorageRow       *row,
                                                                  gboolean             selected);

GtdStorage*                 gtd_storage_row_get_storage          (GtdStorageRow       *row);

G_END_DECLS

#endif /* GTD_GOA_ROW_H */
