/* gtd-object.h
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

#ifndef GTD_OBJECT_H
#define GTD_OBJECT_H

#include <glib-object.h>

#include "gtd-types.h"

G_BEGIN_DECLS

#define GTD_TYPE_OBJECT (gtd_object_get_type())

G_DECLARE_DERIVABLE_TYPE (GtdObject, gtd_object, GTD, OBJECT, GObject)

struct _GtdObjectClass
{
  GObjectClass parent;
};

GtdObject*              gtd_object_new                    (const gchar        *uid);

const gchar*            gtd_object_get_uid                (GtdObject          *object);

void                    gtd_object_set_uid                (GtdObject          *object,
                                                           const gchar        *uid);

gboolean                gtd_object_get_ready              (GtdObject          *object);

void                    gtd_object_set_ready              (GtdObject          *object,
                                                           gboolean            ready);

G_END_DECLS

#endif /* GTD_OBJECT_H */
