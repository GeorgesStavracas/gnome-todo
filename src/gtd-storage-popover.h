/* gtd-storage-popover.h
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

#ifndef GTD_STORAGE_POPOVER_H
#define GTD_STORAGE_POPOVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTD_TYPE_STORAGE_POPOVER (gtd_storage_popover_get_type())

G_DECLARE_FINAL_TYPE (GtdStoragePopover, gtd_storage_popover, GTD, STORAGE_POPOVER, GtkPopover)

GtkWidget*         gtd_storage_popover_new                       (void);

G_END_DECLS

#endif /* GTD_STORAGE_POPOVER_H */
