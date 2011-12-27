/* gWaoN -- gtk+ Spectra Analyzer : about
 * Copyright (C) 2007-2008,2011 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: gwaon-about.c,v 1.4 2011/12/27 14:44:46 kichiki Exp $
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
#include <gtk/gtk.h>
#include <stdlib.h> /* free() */
#include "VERSION.h"

/** global variables **/


void
create_about (void)
{
  GdkPixbuf *logo = gdk_pixbuf_new_from_file ("WaoN.gif", NULL);

  const gchar *authors[] = { 
    "Kengo Ichiki <kichiki@users.sourceforge.net>", 
    NULL 
  };

  GtkWidget *about = g_object_new
    (GTK_TYPE_ABOUT_DIALOG,
     "name", "gWaoN", 
     "version", WAON_VERSION,
     "copyright", "(C) 1998-2008,2011 Kengo Ichiki",
     "comments", "GUI frontend for waon and pv.",
     "authors", authors,
     //"documenters", documenters,
     //"translator-credits", _("translator-credits"),
     "logo", logo,
     "website", "http://waon.sourceforge.net/",
     "website-label", "Visit the WaoN website",
     "license", "GPL\n\n"
     "This program is free software; you can redistribute it and/or modify "
     "it under the terms of the GNU General Public License as published by "
     "the Free Software Foundation; either version 2 of the License, or "
     "(at your option) any later version.\n\n"
     "This program is distributed in the hope that it will be useful, "
     "but WITHOUT ANY WARRANTY; without even the implied warranty of "
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
     "GNU General Public License for more details.\n\n"
     "You should have received a copy of the GNU General Public License "
     "along with this program; if not, write to the Free Software "
     "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.",
     "wrap-license",       TRUE,
     NULL);

  g_object_unref (logo);

  g_signal_connect (about, "response",
		    G_CALLBACK (gtk_widget_destroy),
		    NULL);

  gtk_widget_show_all (about);
}
