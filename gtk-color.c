/* gtk-color.c
 * Copyright (C) 1999,2003,2007 Kengo Ichiki <kichiki@kichiki.com>
 * $Id: gtk-color.c,v 1.2 2007/03/10 20:40:31 kichiki Exp $
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
#include <math.h> /* log10() */
#include <gtk/gtk.h>

#include "gtk-color.h"

/* INPUT
 *   red, green, blue : 0-255
 * OUTPUT
 *   gc :
 */
void
get_color (GtkWidget *widget, gint red, gint green, gint blue, GdkGC *gc)
{
  GdkColor color;

  /* red, green, and blue are passed values, indicating the RGB triple
   * of the color we want to draw. Note that the values of the RGB components
   * within the GdkColor are taken from 0 to 65535, not 0 to 255.
   */
  color.red   = red   * (65535/255);
  color.green = green * (65535/255);
  color.blue  = blue  * (65535/255);
  
  /* the pixel value indicates the index in the colormap of the color.
   * it is simply a combination of the RGB values we set earlier
   */
  color.pixel = (gulong)(red*65536 + green*256 + blue);

  /* However, the pixel valule is only truly valid on 24-bit (TrueColor)
   * displays. Therefore, this call is required so that GDK and X can
   * give us the closest color available in the colormap
   */
  gdk_color_alloc (gtk_widget_get_colormap (widget), &color);

  /* set the foreground to our color */
  gdk_gc_set_foreground (gc, &color);
}


void
get_color_by_scalar (GtkWidget *widget, GdkGC *gc, double scale, int flag_exp)
{
  int i;

  if (flag_exp == 1)
    {
      scale = log10 (scale);
      scale += 12.0;
      if (scale < 0.0)
	{
	  scale = 0.0;
	}
      scale /= 12.0;
    }

  if (scale < 1.0 / 3.0)
    {
      i = (int) (scale * 3.0 * 256);
      get_color (widget, 0, 0, i, gc);
    }
  else if (scale < 2.0 / 3.0)
    {
      i = (int) ((scale * 3.0 - 1.0) * 256);
      get_color (widget, 0, i, 0, gc);
    }
  else
    {
      i = (int) ((scale * 3.0 - 2.0) * 256);
      get_color (widget, i, 0, 0, gc);
    }
}
