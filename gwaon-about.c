/* gWaoN -- gtk+ Spectra Analyzer : about
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: gwaon-about.c,v 1.1 2007/02/09 23:03:59 kichiki Exp $
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


/** global variables **/


/** **/
void
create_about (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *button;

  GtkWidget *frame;
  GtkWidget *label;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroy),
		      GTK_OBJECT (window));
  gtk_widget_set_name (window, "about gWaoN");

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  frame = gtk_frame_new ("gWaoN");
  label = gtk_label_new ("Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>");
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  frame = gtk_frame_new ("... about gWaoN");
  label = gtk_label_new ("gWaoN is a gtk+ GUI frontend for WaoN.");
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  frame = gtk_frame_new ("... about WaoN");
  label = gtk_label_new
    ("WaoN is a Wave-to-Notes transcriber.\n"
     "\"WaoN\" also has a meaning of harmony of notes or chord in Japanese!");
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  frame = gtk_frame_new ("GPL");
  label = gtk_label_new
    ("This program is free software; you can redistribute it and/or modify "
     "it under the terms of the GNU General Public License as published by "
     "the Free Software Foundation; either version 2 of the License, or "
     "(at your option) any later version.\n\n"
     "This program is distributed in the hope that it will be useful, "
     "but WITHOUT ANY WARRANTY; without even the implied warranty of "
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
     "GNU General Public License for more details.\n\n"
     "You should have received a copy of the GNU General Public License "
     "along with this program; if not, write to the Free Software "
     "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.");
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  /** .. And buttons **/
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  /* quit button */
  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (window));


  gtk_widget_show_all (window);
}
