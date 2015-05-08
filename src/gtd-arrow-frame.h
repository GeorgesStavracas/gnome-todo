/* gtd-arrow-frame.h
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

#ifndef GTD_ARROW_FRAME_H
#define GTD_ARROW_FRAME_H

#include "gtd-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTD_TYPE_ARROW_FRAME (gtd_arrow_frame_get_type())

G_DECLARE_FINAL_TYPE (GtdArrowFrame, gtd_arrow_frame, GTD, ARROW_FRAME, GtkFrame)

GtkWidget*                      gtd_arrow_frame_new               (void);

void                            gtd_arrow_frame_set_row           (GtdArrowFrame          *frame,
                                                                   GtdTaskRow             *row);

G_END_DECLS

#endif /* GTD_ARROW_FRAME_H */
