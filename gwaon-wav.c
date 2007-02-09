/* gWaoN -- gtk+ Spectra Analyzer : wav win
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: gwaon-wav.c,v 1.1 2007/02/09 23:03:59 kichiki Exp $
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
#include <gdk/gdkkeysyms.h>
#include <stdio.h> /* for check */
#include <stdlib.h> /* free() */
#include <math.h> // log()

// libsndfile
#include <sndfile.h>
#include "snd.h" // sndfile_read_at()

// FFTW library
#include <fftw3.h>
#include "hc.h"
#include "fft.h" // hanning()

#include "gtk-color.h" /* get_color() */


/** global variables **/
gint WIN_wav_width;
gint WIN_wav_height;

int WIN_wav_cur;
int WIN_wav_scale;
int WIN_wav_mark;


GdkPixmap * wav_pixmap = NULL;
GtkObject * adj_cur;
GtkObject * adj_scale;


int WIN_spec_n;
double *spec_in  = NULL;
double *spec_out = NULL;
double *spec_left  = NULL;
double *spec_right = NULL;
fftw_plan plan;

int flag_window;
double amp2_min, amp2_max;

gint colormap_power_r[256];
gint colormap_power_g[256];
gint colormap_power_b[256];



/** WAV window **/
static void
draw_wav_frame (GtkWidget *widget,
		GdkGC *gc,
		int bottom_wav, int height_wav)
{
  extern GdkPixmap * wav_pixmap;
  extern gint WIN_wav_width;

  int i, j, k;


  // backup GdkFunction
  GdkGCValues backup_gc_values;
  gdk_gc_get_values (gc, &backup_gc_values);

  // set GC wiht XOR function
  gdk_gc_set_function (gc, GDK_XOR);


  // center lines
  get_color (widget, 0, 0, 128, gc); // blue
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom_wav - height_wav/2,
		 WIN_wav_width, bottom_wav - height_wav/2);

  int len = 10000;
  double *left = NULL;
  double *right = NULL;
  left  = (double *)malloc (sizeof (double) * len);
  right = (double *)malloc (sizeof (double) * len);


  extern int WIN_wav_cur;
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;

  int cur_read = WIN_wav_cur;
  for (i = 0; i < len; i ++)
    {
      left [i] = right [i] = 0.0;
    }
  sndfile_read_at (sf, sfinfo, cur_read, left, right, len);


  int iarray = 0;
  int iy;
  int iy_t_l, iy_t_l0 = 0;
  int iy_b_l, iy_b_l0 = 0;
  int iy_t_r, iy_t_r0 = 0;
  int iy_b_r, iy_b_r0 = 0;
  for (i = 0; i < WIN_wav_width; i ++)
    {
      // left channel
      // vertical axis is directing down on the window
      iy = bottom_wav
	- (int)((double)height_wav * (left [iarray] + 1.0) / 2.0);
      iy_t_l = iy_b_l = iy;

      // right channel
      // vertical axis is directing down on the window
      iy = bottom_wav
	- (int)((double)height_wav * (right [iarray] + 1.0) / 2.0);
      iy_t_r = iy_b_r = iy;

      iarray++;
      if (iarray >= len)
	{
	  // read next frame
	  cur_read += iarray;
	  for (k = 0; k < len; k ++) left [k] = right [k] = 0.0;
	  sndfile_read_at (sf, sfinfo, cur_read, left, right, len);
	  // reset iarray
	  iarray = 0;
	}

      for (j = 1; j < WIN_wav_scale; j ++)
	{
	  // left channel
	  // vertical axis is directing down on the window
	  iy = bottom_wav
	    - (int)((double)height_wav * (left [iarray] + 1.0) / 2.0);
	  if (iy_t_l < iy) iy_t_l = iy;
	  if (iy_b_l > iy) iy_b_l = iy;

	  // right channel
	  // vertical axis is directing down on the window
	  iy = bottom_wav
	    - (int)((double)height_wav * (right [iarray] + 1.0) / 2.0);
	  if (iy_t_r < iy) iy_t_r = iy;
	  if (iy_b_r > iy) iy_b_r = iy;

	  iarray++;
	  if (iarray >= len)
	    {
	      // read next frame
	      cur_read += iarray;
	      for (k = 0; k < len; k ++) left [k] = right [k] = 0.0;
	      sndfile_read_at (sf, sfinfo, cur_read, left, right, len);
	      // reset iarray
	      iarray = 0;
	    }
	}
      get_color (widget, 255, 0, 0, gc); // red
      gdk_draw_line (wav_pixmap, gc, i, iy_b_l, i, iy_t_l);
      if (i > 0)
	{
	  gdk_draw_line (wav_pixmap, gc, i-1, iy_t_l0, i, iy_t_l);
	  gdk_draw_line (wav_pixmap, gc, i-1, iy_b_l0, i, iy_b_l);
	}
      iy_t_l0 = iy_t_l;
      iy_b_l0 = iy_b_l;

      get_color (widget, 0, 255, 0, gc); // green
      gdk_draw_line (wav_pixmap, gc, i, iy_b_r, i, iy_t_r);
      if (i > 0)
	{
	  gdk_draw_line (wav_pixmap, gc, i-1, iy_t_r0, i, iy_t_r);
	  gdk_draw_line (wav_pixmap, gc, i-1, iy_b_r0, i, iy_b_r);
	}
      iy_t_r0 = iy_t_r;
      iy_b_r0 = iy_b_r;
    }
  free (left);
  free (right);

  // recover GC's function
  gdk_gc_set_function (gc, backup_gc_values.function);
}

static void
update_win_wav (GtkWidget *widget)
{
  extern gint WIN_wav_width;
  extern gint WIN_wav_height;

  extern int WIN_wav_cur;
  extern int WIN_wav_scale;
  extern int WIN_wav_mark;
  extern int WIN_spec_n;
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;

  GdkGC *gc;
  int i, k;
  int iy;

  if (sf == NULL) return;

  // first, create a GC to draw on
  gc = gdk_gc_new (widget->window);

  // backup GdkFunction
  GdkGCValues backup_gc_values;
  gdk_gc_get_values (gc, &backup_gc_values);


  // find proper dimensions for rectangle
  gdk_window_get_size (widget->window, &WIN_wav_width, &WIN_wav_height);


  // vertical axis is directing down on the window
  int bottom_spec = WIN_wav_height*3/16;
  int height_spec = WIN_wav_height*3/16;

  int bottom_phase = WIN_wav_height*9/32;
  int height_phase = WIN_wav_height*3/32;

  int bottom_spg = WIN_wav_height*29/32;
  int height_spg = WIN_wav_height*5/8;

  int bottom_wav = WIN_wav_height;
  int height_wav = WIN_wav_height*3/32;

  int max_rad_phase = 6;


  // clear image first
  get_color (widget, 0, 0, 0, gc);
  gdk_draw_rectangle (wav_pixmap, gc, TRUE /* fill */,
		      0, 0, 
		      WIN_wav_width, WIN_wav_height);

  // notes
  int ix;
  double x;
  double xmin, xmax;
  //xmin = log (8.175798915644); // log (C-1)
  //xmax = log (12543.853951415975); // log (G9)
  xmin = log (65.406391325150); // log (C2)
  xmax = log (1046.502261202394); // log (C6)
  //xmax = log (2093.004522404788); // log (C7)

  // horizontal line
  get_color (widget, 0, 0, 255, gc); // blue
  // between spec and phase
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom_spec,
		 WIN_wav_width, bottom_spec);
  // bottom of the phase
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom_phase,
		 WIN_wav_width, bottom_phase);
  /*
  // C0
  x = log (16.351597831287);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  // C1
  x = log (32.703195662575);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  // C2
  x = log (65.406391325150);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  */
  // C3
  x = log (130.812782650299);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  // C4
  x = log (261.625565300599);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  // C5
  x = log (523.251130601197);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  /*
  // C6
  x = log (1046.502261202394);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  // C7
  x = log (2093.004522404788);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  // C8
  x = log (4186.009044809573);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  // C9
  x = log (8372.018089619145);
  ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 ix, 0,
		 ix, bottom_phase);
  */


  // WAV window
  draw_wav_frame (widget, gc,
		  bottom_wav, height_wav);

  // parameters for FFT
  extern double amp2_min, amp2_max;
  extern int flag_window;

  double *amp2 = NULL;
  double *phase = NULL;
  amp2  = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
  phase = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));

  double ph_min, ph_max;
  ph_min = - M_PI;
  ph_max = + M_PI;
  int rad;

  double y;


  // Spectrum window
  // set GC wiht OR function
  gdk_gc_set_function (gc, GDK_OR);

  // read the data from the mark
  if (WIN_wav_mark + WIN_spec_n >= (int)sfinfo.frames)
    {
      /* too large WIN_spec_n from WIN_wav_mark */
      sndfile_read_at (sf, sfinfo, WIN_wav_mark,
		       spec_left, spec_right,
		       sfinfo.frames - WIN_wav_mark);
      // zero padding
      for (i = sfinfo.frames - WIN_wav_mark;
	   i < WIN_spec_n; i ++)
	{
	  spec_left [i]  = 0.0;
	  spec_right [i] = 0.0;
	}
    }
  else
    {
      sndfile_read_at (sf, sfinfo, WIN_wav_mark,
		       spec_left, spec_right,
		       WIN_spec_n);
    }

  // left power and phase
  get_color (widget, 255, 0, 0, gc); // red

  windowing (WIN_spec_n, spec_left, flag_window, 1.0, spec_in);
  fftw_execute (plan); // FFT: spec_in[] -> spec_out[]
  HC_to_polar2 (WIN_spec_n, spec_out, 0, (double)WIN_spec_n, amp2, phase);

  for (i = 0; i < (WIN_spec_n/2)+1; i ++) // full span
    {
      x = log ((double)i / (double)WIN_spec_n * sfinfo.samplerate);
      ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
      if (ix < 0 || ix >= WIN_wav_width)
	continue;

      //y = log (amp2 [i]);
      y = log10 (amp2 [i]);
      iy = (int)((double)height_spec * (y - amp2_min) / (amp2_max - amp2_min));
      if (iy < 0) iy = 0;
      gdk_draw_line (wav_pixmap, gc,
		     ix, bottom_spec,
		     ix, bottom_spec - iy);

      // left phase
      iy = bottom_phase
	- (int)((double)height_phase
		* (phase [i] - ph_min) / (ph_max - ph_min));
      rad = (int)((double)max_rad_phase
		  * (y - amp2_min) / (amp2_max - amp2_min));
      if (rad <= 0) rad = 1;
      gdk_draw_arc (wav_pixmap, gc, TRUE, // fill
		    ix-rad/2, iy-rad/2,
		    rad, rad,
		    0, 64*360);
    }

  // right power and phase
  windowing (WIN_spec_n, spec_right, flag_window, 1.0, spec_in);
  fftw_execute (plan); // FFT: spec_in[] -> spec_out[]
  HC_to_polar2 (WIN_spec_n, spec_out, 0, (double)WIN_spec_n, amp2, phase);

  get_color (widget, 0, 255, 0, gc); // green
  for (i = 0; i < (WIN_spec_n/2)+1; i ++) // full span
    {
      x = log ((double)i / (double)WIN_spec_n * sfinfo.samplerate);
      ix = (int)((double)WIN_wav_width * (x - xmin) / (xmax - xmin));
      if (ix < 0 || ix >= WIN_wav_width)
	continue;

      //y = log (amp2 [i]);
      y = log10 (amp2 [i]);
      iy = (int)((double)height_spec * (y - amp2_min) / (amp2_max - amp2_min));
      if (iy < 0) iy = 0;
      gdk_draw_line (wav_pixmap, gc,
		     ix, bottom_spec,
		     ix, bottom_spec - iy);

      // right phase
      iy = bottom_phase
	- (int)((double)height_phase
		* (phase [i] - ph_min) / (ph_max - ph_min));
      rad = (int)((double)max_rad_phase
		  * (y - amp2_min) / (amp2_max - amp2_min));
      if (rad <= 0) rad = 1;
      gdk_draw_arc (wav_pixmap, gc, TRUE, // fill
		    ix-rad/2, iy-rad/2,
		    rad, rad,
		    0, 64*360);
    }
  // recover GC's function
  gdk_gc_set_function (gc, backup_gc_values.function);


  // spectrogram
  extern gint colormap_power_r[256];
  extern gint colormap_power_g[256];
  extern gint colormap_power_b[256];

  int istep;
  istep = WIN_spec_n / WIN_wav_scale;
  if (istep <= 0) istep = 1;

  for (i = 0; i < WIN_wav_width; i+=istep)
    {
      // read data
      for (k = 0; k < WIN_spec_n; k ++)
	{
	  spec_left [k] = spec_right [k] = 0.0;
	}
      sndfile_read_at (sf, sfinfo,
		       WIN_wav_cur + i * WIN_wav_scale,
		       spec_left, spec_right,
		       WIN_spec_n);

      // left + right
      for (k = 0; k < WIN_spec_n; k ++)
	{
	  spec_in [k] = 0.5 * (spec_left [k] + spec_right [k]);
	}
      windowing (WIN_spec_n, spec_in, flag_window, 1.0, spec_in);
      fftw_execute (plan); // FFT: spec_in[] -> spec_out[]
      HC_to_amp2 (WIN_spec_n, spec_out, (double)WIN_spec_n, amp2);

      // drawing
      int ic;
      int ix0 = -1;
      y = 0.0;
      int ny = 0;
      for (k = 0; k < (WIN_spec_n/2)+1; k ++) // full span
	{
	  x = log ((double)k / (double)WIN_spec_n * sfinfo.samplerate);
	  if (x < xmin || x > xmax)
	    continue;

	  ix = (int)((double)height_spg * (x - xmin) / (xmax - xmin));
	  if (ix != ix0)
	    {
	      if (ix0 >= 0)
		{
		  ic = (int)(256.0 * (y - amp2_min) / (amp2_max - amp2_min));
		  if (ic < 0)    ic = 0;
		  if (ic >= 256) ic = 255;
		  get_color (widget,
			     colormap_power_r[ic],
			     colormap_power_g[ic],
			     colormap_power_b[ic],
			     gc);

		  // draw ix0
		  if (ix - ix0 > 1)
		    {
		      // too big gap
		      int ix1;
		      // check
		      x = log ((double)(k-1)
			       / (double)WIN_spec_n * sfinfo.samplerate);
		      ix1 = (int)((double)height_spg
				  * (x - xmin) / (xmax - xmin));
		      if (ix1 != ix0)
			{
			  fprintf (stderr, "something is wrong...\n");
			  exit (1);
			}
		      x = log ((double)(k-2)
			       / (double)WIN_spec_n * sfinfo.samplerate);
		      ix1 = (int)((double)height_spg
				  * (x - xmin) / (xmax - xmin));
		      // so that (ix1, ix0, ix) are the positions
		      // => draw the rectangle for
		      // (ix0-(ix0-ix1)/2, ix0+(ix-ix0)/2) = (ix_b, ix_t)
		      // whose width is ((ix+ix0) - (ix0+ix1))/2 = ix_d
		      int ix_b, ix_t, ix_d;
		      ix_b = (ix0 + ix1)/2;
		      ix_t = (ix  + ix0)/2;
		      ix_d = (ix  - ix1)/2+1;
		      if (ix_b < 0) ix_b = 0;
		      if (ix_d < 1) ix_d = 1;
		      if (ix_t > height_spg) ix_t = height_spg;

		      if (istep <= 1)
			{
			  gdk_draw_line (wav_pixmap, gc,
					 i, bottom_spg - ix_b,
					 i, bottom_spg - ix_t);
			}
		      else
			{
			  if (i + istep >= WIN_wav_width)
			    {
			      gdk_draw_rectangle
				(wav_pixmap, gc,
				 TRUE, // fill
				 i, bottom_spg - ix_t,
				 WIN_wav_width-i, ix_d);
			    }
			  else
			    {
			      gdk_draw_rectangle
				(wav_pixmap, gc,
				 TRUE, // fill
				 i, bottom_spg - ix_t,
				 istep, ix_d);
			    }
			}
		    }
		  else
		    {
		      if (istep <= 1)
			{
			  gdk_draw_point (wav_pixmap, gc, i, bottom_spg - ix0);
			}
		      else
			{
			  if (i + istep >= WIN_wav_width)
			    {
			      gdk_draw_line (wav_pixmap, gc,
					     i,             bottom_spg - ix0,
					     WIN_wav_width, bottom_spg - ix0);
			    }
			  else
			    {
			      gdk_draw_line (wav_pixmap, gc,
					     i,         bottom_spg - ix0,
					     i + istep, bottom_spg - ix0);
			    }
			}
		    }
		}
	      // for ix
	      y = 0.0;
	      ny = 0;
	      ix0 = ix;
	    }
	  //y += log (amp2 [k]); // amp2[] is actually amp^2/WIN_spec_n
	  y += log10 (amp2 [k]); // amp2[] is actually amp^2/WIN_spec_n
	  ny ++;
	}
    }

  // horizontal frequency lines
  get_color (widget, 255, 255, 255, gc); // white
  /*
  // C0
  x = log (16.351597831287);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  // C1
  x = log (32.703195662575);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  // C2
  x = log (65.406391325150);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  */
  // C3
  x = log (130.812782650299);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  // C4
  x = log (261.625565300599);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  // C5
  x = log (523.251130601197);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  /*
  // C6
  x = log (1046.502261202394);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  // C7
  x = log (2093.004522404788);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  // C8
  x = log (4186.009044809573);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  // C9
  x = log (8372.018089619145);
  ix = bottom_spg - (int)((double)height_spg * (x - xmin) / (xmax - xmin));
  gdk_draw_line (wav_pixmap, gc,
		 0, ix,
		 WIN_wav_width, ix);
  */


  free (amp2);
  free (phase);


  // separate line at the top of wav == the bottom of spectrogram
  get_color (widget, 255, 255, 255, gc); // white
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom_spg,
		 WIN_wav_width, bottom_spg);


  // check mark
  // set GC wiht XOR function
  //gdk_gc_set_function (gc, GDK_XOR);
  gdk_gc_set_function (gc, GDK_OR_REVERSE);

  //get_color (widget, 128, 128, 64, gc); // light yellow
  get_color (widget, 128, 128, 128, gc);
  int i0, i1;
  i0 = (WIN_wav_mark              - WIN_wav_cur) / WIN_wav_scale;
  i1 = (WIN_wav_mark + WIN_spec_n - WIN_wav_cur) / WIN_wav_scale;
  if (i0 <= WIN_wav_width && i1 >= 0)
    {
      if (i0 < 0) i0 = 0;
      if (i1 > WIN_wav_width) i1 = WIN_wav_width;
      if ((i1-i0) == 0) i1 = i0 + 1;
      /*
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  i0, WIN_wav_height - (2*height_wav+height_spg),
			  (i1-i0), 2*height_wav+height_spg);
      */
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  i0, WIN_wav_height - (height_wav+height_spg),
			  (i1-i0), height_wav+height_spg);
    }
  // recover GC's function
  gdk_gc_set_function (gc, backup_gc_values.function);


  // finally, destroy GC
  g_object_unref (gc);

  gtk_widget_queue_draw_area (widget, 0, 0, WIN_wav_width, WIN_wav_height);
}

static gboolean
wav_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
  //g_print ("wav_configure_event()\n");

  if (wav_pixmap)
    {
      g_object_unref (wav_pixmap);
    }

  /* allocate pixmap */
  wav_pixmap = gdk_pixmap_new (widget->window,
			       widget->allocation.width,
			       widget->allocation.height,
			       -1);
  update_win_wav (widget);

  return TRUE;
}

static gboolean
wav_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  //g_print ("wav_expose_event()\n");

  gdk_draw_drawable (widget->window,
                     widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                     wav_pixmap,
                     event->area.x, event->area.y,
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);

  return FALSE;
}

static gint
wav_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  extern GdkPixmap * wav_pixmap;
  extern int flag_window;
  extern double amp2_min, amp2_max;

  //fprintf (stderr, "key_press_event\n");
  if (wav_pixmap == NULL)
    {
      return FALSE; /* propogate event */ 
    }

  switch (event->keyval)
    {
    case GDK_Escape:
      break;

    case GDK_Right:
      WIN_spec_n *= 2;
      fprintf (stdout, "Win_spec_n = %d\n", WIN_spec_n);

      free (spec_in);
      free (spec_out);
      fftw_destroy_plan (plan);
      spec_in  = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      spec_out = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      plan = fftw_plan_r2r_1d (WIN_spec_n, spec_in, spec_out,
			       FFTW_R2HC, FFTW_ESTIMATE);
      spec_left  = (double *)realloc (spec_left, sizeof(double) * WIN_spec_n);
      spec_right = (double *)realloc (spec_right, sizeof(double) * WIN_spec_n);

      update_win_wav (widget);
      break;

    case GDK_Left:
      WIN_spec_n /= 2;
      if (WIN_spec_n < 16) WIN_spec_n = 16; // is it enough?
      fprintf (stdout, "Win_spec_n = %d\n", WIN_spec_n);

      free (spec_in);
      free (spec_out);
      fftw_destroy_plan (plan);
      spec_in  = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      spec_out = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      plan = fftw_plan_r2r_1d (WIN_spec_n, spec_in, spec_out,
			       FFTW_R2HC, FFTW_ESTIMATE);
      spec_left  = (double *)realloc (spec_left, sizeof(double) * WIN_spec_n);
      spec_right = (double *)realloc (spec_right, sizeof(double) * WIN_spec_n);

      update_win_wav (widget);
      break;

    case GDK_W:
      flag_window --;
      if (flag_window < 0) flag_window = 6;
      fprint_window_name (stdout, flag_window);
      update_win_wav (widget);
      break;
    case GDK_w:
      flag_window ++;
      if (flag_window > 6) flag_window = 0;
      fprint_window_name (stdout, flag_window);
      update_win_wav (widget);
      break;

    case GDK_Up:
      amp2_max += 1.0;
      fprintf (stdout, "amp2_max = %f\n", amp2_max);
      update_win_wav (widget);
      break;
    case GDK_Down:
      amp2_max -= 1.0;
      fprintf (stdout, "amp2_max = %f\n", amp2_max);
      update_win_wav (widget);
      break;

    case GDK_Page_Up:
      amp2_min += 1.0;
      fprintf (stdout, "amp2_min = %f\n", amp2_min);
      update_win_wav (widget);
      break;
    case GDK_Page_Down:
      amp2_min -= 1.0;
      fprintf (stdout, "amp2_min = %f\n", amp2_min);
      update_win_wav (widget);
      break;

    default:
      return FALSE; /* propogate event */ 
    }

  return TRUE;
}

static gint
wav_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  extern GdkPixmap * wav_pixmap;


  if (wav_pixmap == NULL)
    {
      return FALSE; /* propogate event */ 
    }

  switch (event->button)
    {
    case 1:
      //g_print ("Button Press %.0f, %.0f\n", event->x, event->y);
      WIN_wav_mark = WIN_wav_cur + WIN_wav_scale * event->x;
      update_win_wav (widget);
      break;

    case 2:
      //g_print ("Button2 Press %.0f, %.0f\n", event->x, event->y);
      break;

    case 3:
      //g_print ("Button2 Press %.0f, %.0f\n", event->x, event->y);
      break;

    default:
      return FALSE; /* propogate event */ 
    }

  return TRUE;
}

static gint
wav_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
  extern GdkPixmap * wav_pixmap;
  int x, y;
  GdkModifierType state;

  if (event->is_hint)
    {
      gdk_window_get_pointer (event->window, &x, &y, &state);
    }
  else
    {
      x = event->x;
      y = event->y;
      state = event->state;
    }
    
  if (state & GDK_BUTTON1_MASK && wav_pixmap != NULL)
    //g_print ("Button Motion 1 %d, %d\n", x, y);
  if (state & GDK_BUTTON2_MASK && wav_pixmap != NULL)
    //g_print ("Button Motion 2 %d, %d\n", x, y);
  if (state & GDK_BUTTON3_MASK && wav_pixmap != NULL)
    //g_print ("Button Motion 3 %d, %d\n", x, y);
  if (state & GDK_BUTTON4_MASK && wav_pixmap != NULL)
    //g_print ("Button Motion 4 %d, %d\n", x, y);
  if (state & GDK_BUTTON5_MASK && wav_pixmap != NULL)
    //g_print ("Button Motion 5 %d, %d\n", x, y);
  
  if (state & GDK_BUTTON1_MASK && wav_pixmap != NULL)
    {
      update_win_wav (widget);
    }

  return TRUE;
}

static void
wav_adj_cur (GtkAdjustment *get, GtkAdjustment *set)
{
  extern int WIN_wav_cur;
  extern SF_INFO sfinfo;


  WIN_wav_cur = get->value;
  if (WIN_wav_cur > (int)sfinfo.frames)
    {
      WIN_wav_cur = (int)sfinfo.frames;
    }
  //g_print ("adj_cur() : cur = %d\n", WIN_wav_cur);

  update_win_wav (GTK_WIDGET (set));
}

static void
wav_adj_scale (GtkAdjustment *get, GtkAdjustment *set)
{
  extern int WIN_wav_scale;
  extern SF_INFO sfinfo;


  WIN_wav_scale = get->value;
  if (WIN_wav_cur + WIN_wav_scale > (int)sfinfo.frames)
    {
      WIN_wav_cur = (int)sfinfo.frames - WIN_wav_scale;
      GTK_ADJUSTMENT (adj_cur)->value = (double) WIN_wav_cur;
    }
  //g_print ("adj_scale() : range = %d\n", WIN_wav_scale);

  GTK_ADJUSTMENT (adj_cur)->page_increment
    = (double)(WIN_wav_scale * WIN_wav_width);
  GTK_ADJUSTMENT (adj_cur)->page_size
    = (double)(WIN_wav_scale * WIN_wav_width);

  gtk_adjustment_changed (GTK_ADJUSTMENT (adj_cur));

  update_win_wav (GTK_WIDGET (set));
}


static void
make_color_map (void)
{
  extern gint colormap_power_r[256];
  extern gint colormap_power_g[256];
  extern gint colormap_power_b[256];

  int i;

  for (i = 0; i < 64; i ++)
    {
      colormap_power_r[i] = 0;
      colormap_power_g[i] = i*4;
      colormap_power_b[i] = 255;
    }

  for (i = 64; i < 128; i ++)
    {
      colormap_power_r[i] = 0;
      colormap_power_g[i] = 255;
      colormap_power_b[i] = 255-i*4;
    }

  for (i = 128; i < 192; i ++)
    {
      colormap_power_r[i] = i*4;
      colormap_power_g[i] = 255;
      colormap_power_b[i] = 0;
    }

  for (i = 192; i < 256; i ++)
    {
      colormap_power_r[i] = 255;
      colormap_power_g[i] = 255-i*4;
      colormap_power_b[i] = 0;
    }
}

/** **/
void
create_wav (void)
{
  extern SF_INFO sfinfo;

  extern GtkObject * adj_cur;
  extern GtkObject * adj_scale;

  GtkWidget *window;
  GtkWidget *wav_win;
  GtkWidget *scrollbar;
  GtkWidget *scale;
  GtkWidget *vbox;


  // init params
  extern int WIN_wav_cur;
  extern int WIN_wav_scale;
  extern int WIN_wav_mark;
  WIN_wav_cur = 0;
  WIN_wav_scale = 10;
  WIN_wav_mark = 0;

  make_color_map ();

  // init params
  extern int WIN_spec_n;
  WIN_spec_n = 1024;

  extern double *spec_in;
  extern double *spec_out;
  extern fftw_plan plan;
  spec_in  = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
  spec_out = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
  plan = fftw_plan_r2r_1d (WIN_spec_n, spec_in, spec_out,
			   FFTW_R2HC, FFTW_ESTIMATE);
  extern int flag_window;
  flag_window = 0; // no window
  extern double amp2_min, amp2_max;
  amp2_min = -5.0;
  amp2_max = 1.0;

  extern double *spec_left;
  extern double *spec_right;
  spec_left  = (double *)malloc (sizeof(double) * WIN_spec_n);
  spec_right = (double *)malloc (sizeof(double) * WIN_spec_n);


  /* non-scrolled (plain) window */
  extern gint WIN_wav_width;
  extern gint WIN_wav_height;
  WIN_wav_width = 800;
  WIN_wav_height = 600;
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroy),
		      GTK_OBJECT (window));
  gtk_widget_set_name (window, "gWaoN");
  gtk_widget_set_size_request (GTK_WIDGET (window),
			       WIN_wav_width, WIN_wav_height);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);


  /** WAV window **/
  wav_win = gtk_drawing_area_new ();
  gtk_widget_set_size_request (GTK_WIDGET (wav_win), -1, -1);
  gtk_box_pack_start (GTK_BOX (vbox), wav_win, TRUE, TRUE, 0);
  gtk_widget_show (wav_win);

  /* The following call enables tracking and processing of extension
     events for the drawing area */
  gtk_widget_set_extension_events (wav_win, GDK_EXTENSION_EVENTS_ALL);
  GTK_WIDGET_SET_FLAGS (wav_win, GTK_CAN_FOCUS);
  gtk_widget_grab_focus (wav_win);

  /* Signals used to handle backing pixmap */
  gtk_widget_set_events (wav_win,
			 GDK_EXPOSURE_MASK
                         | GDK_LEAVE_NOTIFY_MASK
                         | GDK_KEY_PRESS_MASK
                         | GDK_BUTTON_PRESS_MASK
                         | GDK_POINTER_MOTION_MASK
                         | GDK_POINTER_MOTION_HINT_MASK);
  /* Signals used to handle backing pixmap */
  gtk_signal_connect (GTK_OBJECT (wav_win), "expose_event",
                      (GtkSignalFunc) wav_expose_event, NULL);
  gtk_signal_connect (GTK_OBJECT(wav_win),"configure_event",
                      (GtkSignalFunc) wav_configure_event, NULL);
  /* Event signals */
  gtk_signal_connect (GTK_OBJECT (wav_win), "motion_notify_event",
                      (GtkSignalFunc) wav_motion_notify_event, NULL);
  gtk_signal_connect (GTK_OBJECT (wav_win), "key_press_event",
                      (GtkSignalFunc) wav_key_press_event, NULL);
  gtk_signal_connect (GTK_OBJECT (wav_win), "button_press_event",
                      (GtkSignalFunc) wav_button_press_event, NULL);


  /* add scrollbar */
  adj_cur = gtk_adjustment_new
    ((double) WIN_wav_cur,
     0.0, // lower
     (double)sfinfo.frames, // upper
     1.0, // step increment
     (double)(WIN_wav_scale * WIN_wav_width), // page increment
     (double)(WIN_wav_scale * WIN_wav_width)); // page size
  g_signal_connect (G_OBJECT (adj_cur), "value_changed",
		    G_CALLBACK (wav_adj_cur), GTK_OBJECT (wav_win));
  scrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (adj_cur));
  gtk_range_set_update_policy (GTK_RANGE (scrollbar),
			       GTK_UPDATE_CONTINUOUS);
  gtk_box_pack_start (GTK_BOX (vbox), scrollbar, FALSE, FALSE, 0);
  gtk_widget_show (scrollbar);


  /* add scale */
  // width = scale * frames
  // scale = frames / width
  adj_scale = gtk_adjustment_new
    ((double) WIN_wav_scale,
     1.0, // lower
     (double)sfinfo.frames/(double)WIN_wav_width, // upper
     1.0, // step increment
     1.0, // page increment
     (double)WIN_wav_width/(double)sfinfo.frames); // page size
  g_signal_connect (G_OBJECT (adj_scale), "value_changed",
		    G_CALLBACK (wav_adj_scale), GTK_OBJECT (wav_win));
  scale = gtk_hscale_new (GTK_ADJUSTMENT (adj_scale));
  gtk_range_set_update_policy (GTK_RANGE (scale),
			       GTK_UPDATE_CONTINUOUS);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);

  // everything is done
  gtk_widget_show (window);
}
