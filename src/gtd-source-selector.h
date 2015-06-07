/* gtd-source-selector.h
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

#ifndef GTD_SOURCE_SELECTOR_H
#define GTD_SOURCE_SELECTOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GTD_TYPE_SOURCE_SELECTOR (gtd_source_selector_get_type())

G_DECLARE_DERIVABLE_TYPE (GtdSourceSelector, gtd_source_selector, GTD, SOURCE_SELECTOR, GtkBox)

struct _GtdSourceSelectorClass
{
  GtkBoxClass parent;
};

GtdSourceSelector *gtd_source_selector_new (void);

G_END_DECLS

#endif /* GTD_SOURCE_SELECTOR_H */
