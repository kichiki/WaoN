/* header file for gtk-color.c --
 * gtk-color.c
 * Copyright (C) 1999,2003,2007 Kengo Ichiki <kichiki@kichiki.com>
 * $Id: gtk-color.h,v 1.1 2007/02/09 23:03:59 kichiki Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef	_GTK_COLOR_H_
#define	_GTK_COLOR_H_


/* INPUT
 *   red, green, blue : 0-255
 * OUTPUT
 *   gc :
 */
void
get_color (GtkWidget *widget, gint red, gint green, gint blue, GdkGC *gc);

void
get_color_by_scalar (GtkWidget *widget, GdkGC *gc, double scale, int flag_exp);


#endif /* !_GTK_COLOR_H_ */
