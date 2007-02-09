/* gWaoN -- gtk+ Spectra Analyzer : menu
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: gwaon-menu.c,v 1.1 2007/02/09 23:03:59 kichiki Exp $
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
#include <stdio.h> /* fprintf() */
#include <stdlib.h> /* malloc() */
#include <fcntl.h> /* open() */
#include <unistd.h> /* close() */
#include <gtk/gtk.h>

#include <string.h> // memset()

// libsndfile
#include <sndfile.h>

#include "gwaon-about.h" /* create_about() */
#include "gwaon-wav.h" /* create_wav() */


static void
on_quit (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

static void
on_preference (void)
{
}

/* Get the selected filename and print it to the console */
static void
on_file_open_wav_sel_ok (GtkWidget *widget, GtkFileSelection *fs)
{
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;
  gchar *filename;

  filename =
    (gchar *) gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
  g_print ("%s\n", filename);


  // close sf first, if sf is open
  if (sf != NULL)
    {
      sf_close (sf);
    }

  // open the file
  memset (&sfinfo, 0, sizeof (sfinfo));
  sf = sf_open (filename, SFM_READ, &sfinfo);
  if (sf == NULL)
    {
      g_print ("fail to open %s\n", filename);
      return;
    }

  g_print ("channels = %d\n", sfinfo.channels);
  g_print ("samplerate = %d\n", sfinfo.samplerate);
  g_print ("frames = %d\n", (int)sfinfo.frames);

  create_wav ();

  gtk_widget_destroy (GTK_WIDGET (fs));
}

static void
on_file_open_wav (GtkWidget *widget, gpointer data)
{
  GtkWidget *filew;

  /* Create a new file selection widget */
  filew = gtk_file_selection_new ("File selection");

  gtk_signal_connect (GTK_OBJECT (filew), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroy),
		      GTK_OBJECT (filew));
  /* Connect the ok_button to file_ok_sel function */
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		      "clicked",
		      (GtkSignalFunc) on_file_open_wav_sel_ok, filew );

  /* Connect the cancel_button to destroy the widget */
  gtk_signal_connect_object (GTK_OBJECT 
			     (GTK_FILE_SELECTION (filew)->cancel_button),
			     "clicked", (GtkSignalFunc) gtk_widget_destroy,
			     GTK_OBJECT (filew));

  /* Lets set the filename, as if this were a save dialog, and we are giving
     a default filename */
  gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), 
				   "test.wav");

  gtk_widget_show(filew);
}

void
create_menu (void)
{
  // create a new window
  GtkWidget *window;
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /*gtk_widget_set_usize( GTK_WIDGET (window), 200, 100);*/
  gtk_widget_set_uposition (window, 200, 100);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroy),
		      GTK_OBJECT (window));
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		      GTK_SIGNAL_FUNC (on_quit), NULL);

  /*gtk_signal_connect_after (GTK_OBJECT (window), "key_press_event",
			    GTK_SIGNAL_FUNC (on_key_press_event), NULL);
  gtk_signal_connect (GTK_OBJECT (window), "expose_event",
		      GTK_SIGNAL_FUNC (on_expose_event), NULL);*/

  gtk_widget_realize (window);

  gtk_window_set_title(GTK_WINDOW (window), "gWaoN");
  gtk_container_border_width (GTK_CONTAINER (window), 0);

  GtkTooltips *tooltips;
  tooltips = gtk_tooltips_new ();
  GtkAccelGroup *accel_group;
  /*accel_group = gtk_accel_group_get_default ();*/
  accel_group = gtk_accel_group_new (); /* for gtk-2.0 */


  GtkWidget *vbox;
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);


  /* create menu bar */
  GtkWidget *menubar;
  menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
  gtk_widget_show (menubar);

  /* create File menu */
  GtkWidget *menuitem;
  menuitem = gtk_menu_item_new_with_label ("File");
  gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
  gtk_widget_show (menuitem);

  GtkWidget *menu;
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  /* File -> Open WAV */
  menuitem = gtk_menu_item_new_with_label ("Open WAV");
  gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
		      GTK_SIGNAL_FUNC (on_file_open_wav), NULL);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      'O', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  gtk_menu_append (GTK_MENU (menu), menuitem);
  gtk_tooltips_set_tip (tooltips, menuitem,
			"Open an existing project", NULL);
  gtk_widget_show (menuitem);

  /* --separator-- */
  menuitem = gtk_menu_item_new ();
  gtk_menu_append (GTK_MENU (menu), menuitem);
  gtk_widget_show (menuitem);

  /* File -> Preferences */
  menuitem = gtk_menu_item_new_with_label ("Preferences");
  gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
		             GTK_SIGNAL_FUNC (on_preference),
			     NULL);
  gtk_menu_append (GTK_MENU (menu), menuitem);
  gtk_tooltips_set_tip (tooltips, menuitem, "Edit project preferences",
			NULL);
  gtk_widget_show (menuitem);

  /* --separator-- */
  menuitem = gtk_menu_item_new ();
  gtk_menu_append (GTK_MENU (menu), menuitem);
  gtk_widget_show (menuitem);

  /* File -> Exit */
  menuitem = gtk_menu_item_new_with_label ("Exit");
  gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
		             GTK_SIGNAL_FUNC (on_quit),
			     NULL);
  gtk_menu_append (GTK_MENU (menu), menuitem);
  gtk_tooltips_set_tip (tooltips, menuitem, "Exit gWaoN",
			NULL);
  gtk_widget_show (menuitem);

  /* Create Help menu */
  menuitem = gtk_menu_item_new_with_label ("Help");
  gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
  gtk_widget_show (menuitem);

  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  /* Help -> About */
  menuitem = gtk_menu_item_new_with_label ("About...");
  gtk_menu_append (GTK_MENU (menu), menuitem);
  gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
		      GTK_SIGNAL_FUNC (create_about), NULL);
  gtk_widget_show (menuitem);


  gtk_widget_show (window);
}
