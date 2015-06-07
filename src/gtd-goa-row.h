/* gtd-goa-row.h
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

#define GTD_TYPE_GOA_ROW (gtd_goa_row_get_type())

G_DECLARE_FINAL_TYPE (GtdGoaRow, gtd_goa_row, GTD, GOA_ROW, GtkListBoxRow)

GtkWidget*                  gtd_goa_row_new                      (GoaAccount          *account);

GoaAccount*                 gtd_goa_row_get_account              (GtdGoaRow           *row);

gboolean                    gtd_goa_row_get_selected             (GtdGoaRow           *row);

void                        gtd_goa_row_set_selected             (GtdGoaRow           *row,
                                                                  gboolean             selected);

G_END_DECLS

#endif /* GTD_GOA_ROW_H */
