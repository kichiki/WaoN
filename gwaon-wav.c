/* gWaoN -- gtk+ Spectra Analyzer : wav win
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: gwaon-wav.c,v 1.12 2007/03/11 01:35:53 kichiki Exp $
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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h> /* for check */
#include <stdlib.h> /* free() */
#include <string.h> // strcpy()
#include <math.h> // log()

// libsndfile
#include <sndfile.h>
#include "snd.h" // sndfile_read_at()

// FFTW library
#include <fftw3.h>
#include "hc.h"
#include "fft.h" // hanning()
#include "midi.h" // midi_to_freq(), etc.

#include "gwaon-play.h" // play_1msec()
// ao device
#include <ao/ao.h>
#include "ao-wrapper.h"

#include "pv-complex.h" // struct pv_complex_data
#include "gtk-color.h" /* get_color() */
#include "memory-check.h" // CHECK_MALLOC() macro


// global variables
gint WIN_wav_width;
gint WIN_wav_height;

int WIN_wav_cur;
int WIN_wav_scale;
int WIN_wav_mark;


GdkPixmap *wav_pixmap = NULL;
GtkObject *adj_cur;
GtkObject *adj_scale;

GdkPixbuf *wav_pixbuf = NULL;


int WIN_spec_n;
int WIN_spec_hop_scale;
int WIN_spec_hop;
int WIN_spec_mode;
double *spec_in  = NULL;
double *spec_out = NULL;
double *spec_left  = NULL;
double *spec_right = NULL;
fftw_plan plan;

int flag_window;
double amp2_min, amp2_max;

int oct_min, oct_max;
double logf_min, logf_max;


gint colormap_power_r[256];
gint colormap_power_g[256];
gint colormap_power_b[256];



/* draw wave panel
 * INPUT
 *  i0, i1 : the range to draw in the display (pixel)
 */
static void
draw_wav_frame (GtkWidget *widget,
		GdkGC *gc,
		int bottom_wav, int height_wav,
		int i0, int i1)
{
  extern GdkPixmap *wav_pixmap;

  int i, j, k;


  // clear image first
  get_color (widget, 0, 0, 0, gc);
  gdk_draw_rectangle (wav_pixmap, gc, TRUE /* fill */,
		      i0, bottom_wav - height_wav, 
		      i1, height_wav);

  // backup GdkFunction
  GdkGCValues backup_gc_values;
  gdk_gc_get_values (gc, &backup_gc_values);

  // set GC wiht XOR function
  gdk_gc_set_function (gc, GDK_XOR);


  // clear image first
  get_color (widget, 0, 0, 0, gc);
  gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
		      i0, bottom_wav - height_wav, 
		      i1, height_wav);

  // center lines
  get_color (widget, 0, 0, 128, gc); // blue
  gdk_draw_line (wav_pixmap, gc,
		 i0, bottom_wav - height_wav/2,
		 i1, bottom_wav - height_wav/2);

  extern int WIN_wav_cur;
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;


  // the following is the dynamic
  if (sf != NULL)
    {
      int len = 10000;
      double *left = NULL;
      double *right = NULL;
      left  = (double *)malloc (sizeof (double) * len);
      right = (double *)malloc (sizeof (double) * len);
      CHECK_MALLOC (left,  "draw_wav_frame");
      CHECK_MALLOC (right, "draw_wav_frame");

      int cur_read = WIN_wav_cur + i0 * WIN_wav_scale;
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
      for (i = i0; i < i1; i ++)
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
  // the following is static contents
}

/*
 * OUTPUT (returnd value)
 *  ix : ranges from 0 to resolution for x = lofg_min to logf_max
 */
static int
logf_to_display (double logf, int resolution)
{
  extern double logf_min, logf_max;
  int ix;
  ix = (int)((double)resolution * (logf - logf_min) / (logf_max - logf_min));
  return (ix);
}

/*
 * OUTPUT (returnd value)
 *  ix : ranges from 0 to resolution for x = logf_min to logf_max
 */
static double
display_to_logf (int ix, int resolution)
{
  extern double logf_min, logf_max;
  double logf;
  logf = logf_min
    + ((double)ix + 0.5) * (logf_max - logf_min) / (double)resolution;
  return (logf);
}

/* return the bottom position on the display for the midi note
 * OUTPUT (returnd value)
 *  ix : ranges from 0 to resolution for x = logf_min to logf_max
 */
static int
midi_to_display_bottom (int midi, int res)
{
  extern double logf_min, logf_max;
  int ix0, ix1;
  ix0 = logf_to_display (midi_to_logf (midi-1), res);
  ix1 = logf_to_display (midi_to_logf (midi),   res);
  return ((ix1 + ix0) / 2);
}
/* return the top position on the display for the midi note
 * OUTPUT (returnd value)
 *  ix : ranges from 0 to resolution for x = logf_min to logf_max
 */
static int
midi_to_display_top (int midi, int res)
{
  extern double logf_min, logf_max;
  return (midi_to_display_bottom (midi+1, res));
}

/*
 *  flag_ori : 0 vertical
 *             1 horizontal
 */
static void
draw_keyboard (GtkWidget *widget,
	       GdkGC *gc,
	       int bottom, int height)
{
  extern GdkPixmap *wav_pixmap;

  extern int oct_min, oct_max;
  extern double logf_min, logf_max;

  int i, j;
  int ix;
  double x;

  int idx;
  idx = (int)((double)height / (double)(oct_max - oct_min) / 12.0);

  get_color (widget, 255, 255, 255, gc); // white
  gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
		      0, bottom - height,
		      10, height);
  get_color (widget, 0, 0, 0, gc); // black
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom - height,
		 0, bottom);
  gdk_draw_line (wav_pixmap, gc,
		 10, bottom - height,
		 10, bottom);

  for (i = oct_min; i < oct_max; i ++)
    {
      // C# (Db)
      j = 1;
      ix = bottom - midi_to_display_top ((i+1)*12+j, height);
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  0, ix,
			  6, idx);
      // D# (Eb)
      j = 3;
      ix = bottom - midi_to_display_top ((i+1)*12+j, height);
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  0, ix,
			  6, idx);
      // F# (Gb)
      j = 6;
      ix = bottom - midi_to_display_top ((i+1)*12+j, height);
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  0, ix,
			  6, idx);
      // G# (Ab)
      j = 8;
      ix = bottom - midi_to_display_top ((i+1)*12+j, height);
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  0, ix,
			  6, idx);
      // A# (Bb)
      j = 10;
      ix = bottom - midi_to_display_top ((i+1)*12+j, height);
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  0, ix,
			  6, idx);

      // on the black key of C#
      j = 1;
      x = midi_to_logf ((i+1)*12+j);
      ix = bottom
	- (int)((double)height * (x - logf_min) / (logf_max - logf_min));
      gdk_draw_line (wav_pixmap, gc,
		     0, ix,
		     9, ix);
      // on the black key of D#
      j = 3;
      x = midi_to_logf ((i+1)*12+j);
      ix = bottom
	- (int)((double)height * (x - logf_min) / (logf_max - logf_min));
      gdk_draw_line (wav_pixmap, gc,
		     0, ix,
		     9, ix);
      // on the black key of F#
      j = 6;
      x = midi_to_logf ((i+1)*12+j);
      ix = bottom
	- (int)((double)height * (x - logf_min) / (logf_max - logf_min));
      gdk_draw_line (wav_pixmap, gc,
		     0, ix,
		     9, ix);
      // on the black key of G#
      j = 8;
      x = midi_to_logf ((i+1)*12+j);
      ix = bottom
	- (int)((double)height * (x - logf_min) / (logf_max - logf_min));
      gdk_draw_line (wav_pixmap, gc,
		     0, ix,
		     9, ix);
      // on the black key of A#
      j = 10;
      x = midi_to_logf ((i+1)*12+j);
      ix = bottom
	- (int)((double)height * (x - logf_min) / (logf_max - logf_min));
      gdk_draw_line (wav_pixmap, gc,
		     0, ix,
		     9, ix);
      // between E and F
      j = 4;
      ix = bottom - midi_to_display_top ((i+1)*12+j, height);
      gdk_draw_line (wav_pixmap, gc,
		     0, ix,
		     9, ix);
      // between B and C
      j = 11;
      ix = bottom - midi_to_display_top ((i+1)*12+j, height);
      gdk_draw_line (wav_pixmap, gc,
		     0, ix,
		     9, ix);
    }
}

/*
 * INPUT
 *  drop : positive => make drop shadow by (rd, gd, bd)
 *         zero     => no drop shadow
 */
static void
draw_text (GtkWidget *widget,
	   GdkGC *gc,
	   gint x, gint y,
	   const gchar *text,
	   gint r, gint g, gint b,
	   gint drop,
	   gint rd, gint gd, gint bd,
	   const gchar *font)
{
  PangoLayout *layout;
  layout = gtk_widget_create_pango_layout (widget, text);

  PangoFontDescription *fontdesc;
  fontdesc = pango_font_description_from_string (font);

  pango_layout_set_font_description (layout, fontdesc); 
  pango_font_description_free (fontdesc);

  if (drop > 0)
    {
      get_color (widget, rd, gd, bd, gc);
      gdk_draw_layout (wav_pixmap, gc, x+drop, y+drop, layout);
    }

  get_color (widget, r, g, b, gc);
  gdk_draw_layout (wav_pixmap, gc, x, y, layout);

  g_object_unref (layout);
}

void draw_infos (GtkWidget *widget,
		 GdkGC *gc,
		 int y_spec_top, int y_spec_bottom,
		 gint r, gint g, gint b,
		 gint drop,
		 gint rd, gint gd, gint bd,
		 const gchar *font)
{
  extern int WIN_spec_n;
  extern int WIN_spec_hop;
  extern int WIN_spec_mode;
  extern int flag_window;
  extern double amp2_min, amp2_max;
  extern int oct_min, oct_max;
  //extern struct pv_complex_data *pv;

  gchar string [256];


  // informations
  sprintf (string, "N = %d", WIN_spec_n);
  draw_text (widget, gc, 100, 0, string,
	     r, g, b, drop, rd, gd, bd, font);
  sprintf (string, "H = %d (%d)", WIN_spec_hop, WIN_spec_hop_scale);
  draw_text (widget, gc, 280, 0, string,
	     r, g, b, drop, rd, gd, bd, font);

  // spectrum mode
  switch (WIN_spec_mode)
    {
    case 0: // plain FFT
      strcpy (string, "plain FFT");
      break;

    case 1: // FFT w PV suggestions
      strcpy (string, "FFT w PV");
      break;

    case 2: // PV-corrected Spectrum
      strcpy (string, "PV-FFT");
      break;
    }
  draw_text (widget, gc, 400, 0, string,
	     r, g, b, drop, rd, gd, bd, font);

  // window
  switch (flag_window)
    {
    case 0: // square (no window)
      strcpy (string, "square");
      break;

    case 1: // parzen window
      strcpy (string, "parzen");
      break;

    case 2: // welch window
      strcpy (string, "welch");
      break;

    case 3: // hanning window
      strcpy (string, "hanning");
      break;

    case 4: // hamming window
      strcpy (string, "hamming");
      break;

    case 5: // blackman window
      strcpy (string, "blackman");
      break;

    case 6: // steeper 30-dB/octave rolloff window
      strcpy (string, "steeper");
      break;
    }
  draw_text (widget, gc, 200, 0, string,
	     r, g, b, drop, rd, gd, bd, font);

  // phase lock
  /*
  if (pv->flag_lock == 0)
    {
      strcpy (string, "no lock");
    }
  else
    {
      strcpy (string, "loose lock");
    }
  draw_text (widget, gc, 480, 0, string,
	     r, g, b, drop, rd, gd, bd, font);
  */


  sprintf (string, "10^%.0f", amp2_max);
  draw_text (widget, gc, 10, y_spec_top, string,
	     r, g, b, drop, rd, gd, bd, font);
  sprintf (string, "10^%.0f", amp2_min);
  draw_text (widget, gc, 10, y_spec_bottom-15, string,
	     r, g, b, drop, rd, gd, bd, font);

  sprintf (string, "(Oct:%d)", oct_max-1);
  draw_text (widget, gc,
	     WIN_wav_width - 50, 0, string,
	     r, g, b, drop, rd, gd, bd, font);
}

/*
 * INPUT
 *  i : starting frame to analyse
 *  r_amp2 : if NULL is given, l+r are returned in l_amp2 (and l_ph)
 *  ph : if NULL is given, just l_amp2 (and r_amp2) is calculated.
 * OUTPUT
 *  l_amp2, r_amp2 : amp^2 scaled by WIN_spec_n.
 *  l_ph, r_ph   : phase if ph is not NULL
 */
static void
fft_one_frame (int i,
	       double *l_amp2, double *r_amp2,
	       double *l_ph,   double *r_ph)
{
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;

  extern int WIN_spec_n;
  extern double *spec_left;
  extern double *spec_right;
  extern fftw_plan plan;

  int k;

  // read data
  for (k = 0; k < WIN_spec_n; k ++)
    {
      spec_left [k] = spec_right [k] = 0.0;
    }
  sndfile_read_at (sf, sfinfo,
		   i,
		   spec_left, spec_right,
		   WIN_spec_n);

  if (r_amp2 == NULL)
    {
      // left + right
      for (k = 0; k < WIN_spec_n; k ++)
	{
	  spec_in [k] = 0.5 * (spec_left [k] + spec_right [k]);
	}
      windowing (WIN_spec_n, spec_in, flag_window, 1.0, spec_in);
      fftw_execute (plan); // FFT: spec_in[] -> spec_out[]
      if (l_ph == NULL)
	{
	  HC_to_amp2 (WIN_spec_n, spec_out, (double)WIN_spec_n,
		      l_amp2);
	}
      else
	{
	  HC_to_polar2 (WIN_spec_n, spec_out, 0, (double)WIN_spec_n,
			l_amp2, l_ph);
	}
    }
  else
    {
      // left
      windowing (WIN_spec_n, spec_left, flag_window, 1.0, spec_in);
      fftw_execute (plan); // FFT: spec_in[] -> spec_out[]
      if (l_ph == NULL)
	{
	  HC_to_amp2 (WIN_spec_n, spec_out, (double)WIN_spec_n,
		      l_amp2);
	}
      else
	{
	  HC_to_polar2 (WIN_spec_n, spec_out, 0, (double)WIN_spec_n,
			l_amp2, l_ph);
	}

      // right
      windowing (WIN_spec_n, spec_right, flag_window, 1.0, spec_in);
      fftw_execute (plan); // FFT: spec_in[] -> spec_out[]
      if (l_ph == NULL)
	{
	  HC_to_amp2 (WIN_spec_n, spec_out, (double)WIN_spec_n,
		      r_amp2);
	}
      else
	{
	  HC_to_polar2 (WIN_spec_n, spec_out, 0, (double)WIN_spec_n,
			r_amp2, r_ph);
	}
    }
}

/*
 * INPUT
 *  r_amp2 : if NULL is given, left+right is analised.
 *  l_dphi, r_dphi : 
 *    dphi[k] = principal(ph2 - ph1 - Omega[k]),
 *    where principal() rounds the value in [-pi, pi]
 *    and Omega[k] = 2pi k hop / N.
 *    NOTE the real frequency of k-bin in Hz is, therefore,
 *    (k + [lr]_f * N) * samplerate / N
 *
 *    = (k / N + [lr]_f) * samplerate
 */
static void
fft_two_frames (int i, int hop,
		double *l_amp2, double *r_amp2,
		double *l_ph,   double *r_ph,
		double *l_dphi, double *r_dphi)
{
  double *l_ph1 = NULL;
  double *r_ph1 = NULL;
  l_ph1 = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
  r_ph1 = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
  CHECK_MALLOC (l_ph1, "fft_two_frames");
  CHECK_MALLOC (r_ph1, "fft_two_frames");

  fft_one_frame (i + hop, l_amp2, r_amp2, l_ph1, r_ph1);
  fft_one_frame (i,       l_amp2, r_amp2, l_ph,  r_ph);
  // because we need [lr]_amp2 at the frame i

  // freq correction by phase difference
  double twopi = 2.0 * M_PI;
  for (i = 0; i < (WIN_spec_n/2)+1; ++i) // full span
    {
      double dphi;
      // left
      dphi = l_ph1[i] - l_ph[i]
	- twopi * (double)i / (double)WIN_spec_n * (double)hop;
      for (; dphi >= M_PI; dphi -= twopi);
      for (; dphi < -M_PI; dphi += twopi);
      l_dphi [i] = dphi / twopi / (double)hop;

      // right
      if (r_amp2 != NULL)
	{
	  dphi = r_ph1[i] - r_ph[i]
	    - twopi * (double)i / (double)WIN_spec_n * (double)hop;
	  for (; dphi >= M_PI; dphi -= twopi);
	  for (; dphi < -M_PI; dphi += twopi);
	  r_dphi [i] = dphi / twopi / (double)hop;
	}
    }

  free (l_ph1);
  free (r_ph1);
}

/*
 * amp [resolution] : note that this is not squared.
 */
static void
average_PV_FFT (int resolution,
		const double *amp2, const double *dphi,
		double *amp)
{
  extern int WIN_spec_n;
  extern SF_INFO sfinfo;
  extern double logf_min, logf_max;

  int i;
  for (i = 0; i < resolution; i ++)
    {
      amp [i] = 0.0;
    }

  int k;
  double x;
  int ix;
  for (k = 1; k < (WIN_spec_n+1)/2; k ++)
    {
      // freq -> display position
      x = log (((double)k / (double)WIN_spec_n + dphi [k])
	       * sfinfo.samplerate);
      ix = (int)((double)resolution * (x - logf_min) / (logf_max - logf_min));
      if (ix < 0 || ix >= resolution)
	continue;

      amp [ix] += sqrt (amp2 [k]);
    }
}


/* draw spectrum panel (power and phase spectra)
 * INPUT
 */
static void
draw_spectrum_frame (GtkWidget *widget,
		     GdkGC *gc,
		     int bottom_spec, int height_spec,
		     int bottom_phase, int height_phase)
{
  extern GdkPixmap *wav_pixmap;
  extern gint WIN_wav_width;
  extern int WIN_spec_mode;
  extern double amp2_min, amp2_max;
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;

  int i;

  double x;
  int ix, ix0;
  double y;
  int iy;
  int ic;

  int max_rad_phase = 6;

  double ph_min, ph_max;
  ph_min = - M_PI;
  ph_max = + M_PI;
  int rad;


  // clear image first
  get_color (widget, 0, 0, 0, gc);
  // spec
  gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
		      0, bottom_spec - height_spec, 
		      WIN_wav_width, height_spec);
  // phase
  gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
		      0, bottom_phase - height_phase, 
		      WIN_wav_width, height_phase);


  // center of the phase (zero)
  get_color (widget, 0, 0, 255, gc); // blue
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom_phase - height_phase/2,
		 WIN_wav_width, bottom_phase - height_phase/2);

  // draw lines separating octaves for spectrum and phase panels
  // i is the midi octave
  for (i = oct_min + 1; i < oct_max; i ++)
    {
      ix = logf_to_display (midi_to_logf ((i+1)*12),
			    WIN_wav_width);
      gdk_draw_line (wav_pixmap, gc,
		     ix, 0,
		     ix, bottom_phase);
    }


  // backup GdkFunction
  GdkGCValues backup_gc_values;
  gdk_gc_get_values (gc, &backup_gc_values);


  // the following is the dynamic
  if (sf != NULL)
    {
      // allocate working area
      double *l_amp2 = NULL;
      double *r_amp2 = NULL;
      l_amp2  = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
      r_amp2  = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));

      double *l_ph = NULL;
      double *r_ph = NULL;
      l_ph = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
      r_ph = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));

      double *l_dphi = NULL;
      double *r_dphi = NULL;
      l_dphi = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
      r_dphi = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));

      CHECK_MALLOC (l_amp2, "draw_spectrum_frame");
      CHECK_MALLOC (r_amp2, "draw_spectrum_frame");
      CHECK_MALLOC (l_ph,   "draw_spectrum_frame");
      CHECK_MALLOC (r_ph,   "draw_spectrum_frame");
      CHECK_MALLOC (l_dphi, "draw_spectrum_frame");
      CHECK_MALLOC (r_dphi, "draw_spectrum_frame");


      extern int oct_max, oct_min;
      int resolution = (oct_max - oct_min) * 12;
      double *l_ave = NULL;
      double *r_ave = NULL;
      if (WIN_spec_mode == 2)
	{
	  l_ave = (double *)malloc (sizeof (double) * resolution);
	  r_ave = (double *)malloc (sizeof (double) * resolution);
	  CHECK_MALLOC (l_ave, "draw_spectrum_frame");
	  CHECK_MALLOC (r_ave, "draw_spectrum_frame");
	}

      // set GC wiht OR function
      gdk_gc_set_function (gc, GDK_OR);

      // analyse the frame WIN_wav_mark
      if (WIN_spec_mode == 0)
	{
	  fft_one_frame (WIN_wav_mark,
			 l_amp2, r_amp2,
			 l_ph, r_ph);
	}
      else
	{
	  fft_two_frames (WIN_wav_mark, WIN_spec_hop,
			  l_amp2, r_amp2,
			  l_ph, r_ph,
			  l_dphi, r_dphi);
	  if (WIN_spec_mode == 2)
	    {
	      average_PV_FFT (resolution, l_amp2, l_dphi, l_ave);
	      average_PV_FFT (resolution, r_amp2, r_dphi, r_ave);
	    }
	}

      // left power and phase
      for (i = 0; i < (WIN_spec_n/2)+1; i ++) // full span
	{
	  x = log ((double)i / (double)WIN_spec_n * sfinfo.samplerate);
	  ix = logf_to_display (x, WIN_wav_width);
	  if (ix < 0 || ix >= WIN_wav_width)
	    continue;

	  // left power
	  y = log10 (l_amp2 [i]);
	  iy = (int)((double)height_spec * (y - amp2_min)
		     / (amp2_max - amp2_min));
	  if (iy < 0) iy = 0;

	  if (WIN_spec_mode == 0)
	    {
	      get_color (widget, 255, 0, 0, gc); // red
	      gdk_draw_line (wav_pixmap, gc,
			     ix, bottom_spec,
			     ix, bottom_spec - iy);
	    }
	  else if (WIN_spec_mode == 1)
	    {
	      get_color (widget, 255, 0, 0, gc); // red
	      gdk_draw_line (wav_pixmap, gc,
			     ix, bottom_spec,
			     ix, bottom_spec - iy);

	      // log of the corrected freq
	      get_color (widget, 255, 0, 255, gc);
	      x = log (((double)i / (double)WIN_spec_n + l_dphi [i])
		       * sfinfo.samplerate);
	      ix0 = logf_to_display (x, WIN_wav_width);
	      gdk_draw_line (wav_pixmap, gc,
			     ix,  bottom_spec - iy,
			     ix0, bottom_spec - iy);
	      gdk_draw_arc (wav_pixmap, gc, TRUE, // fill
			    ix0-2, (bottom_spec - iy)-2,
			    4, 4,
			    0, 64*360);
	    }
	  else if (WIN_spec_mode == 2)
	    {
	      get_color (widget, 255, 0, 0, gc); // red
	      // log of the corrected freq
	      x = log (((double)i / (double)WIN_spec_n + l_dphi [i])
		       * sfinfo.samplerate);
	      ix0 = logf_to_display (x, WIN_wav_width);
	      gdk_draw_line (wav_pixmap, gc,
			     ix0, bottom_spec,
			     ix0, bottom_spec - iy);
	    }

	  // left phase
	  get_color (widget, 255, 0, 0, gc); // red
	  iy = bottom_phase
	    - (int)((double)height_phase
		    * (l_ph [i] - ph_min) / (ph_max - ph_min));
	  rad = (int)((double)max_rad_phase
		      * (y - amp2_min) / (amp2_max - amp2_min));
	  if (rad <= 0) rad = 1;
	  gdk_draw_arc (wav_pixmap, gc, TRUE, // fill
			ix-rad/2, iy-rad/2,
			rad, rad,
			0, 64*360);


	  // right power
	  y = log10 (r_amp2 [i]);
	  iy = (int)((double)height_spec * (y - amp2_min)
		     / (amp2_max - amp2_min));
	  if (iy < 0) iy = 0;

	  if (WIN_spec_mode == 0)
	    {
	      get_color (widget, 0, 255, 0, gc); // green
	      gdk_draw_line (wav_pixmap, gc,
			     ix, bottom_spec,
			     ix, bottom_spec - iy);
	    }
	  else if (WIN_spec_mode == 1)
	    {
	      get_color (widget, 0, 255, 0, gc); // green
	      gdk_draw_line (wav_pixmap, gc,
			     ix, bottom_spec,
			     ix, bottom_spec - iy);

	      // log of the corrected freq
	      get_color (widget, 0, 255, 255, gc);
	      x = log (((double)i / (double)WIN_spec_n + r_dphi [i])
		       * sfinfo.samplerate);
	      ix0 = logf_to_display (x, WIN_wav_width);
	      gdk_draw_line (wav_pixmap, gc,
			     ix,  bottom_spec - iy,
			     ix0, bottom_spec - iy);
	      gdk_draw_arc (wav_pixmap, gc, TRUE, // fill
			    ix0-2, (bottom_spec - iy)-2,
			    4, 4,
			    0, 64*360);
	    }
	  else if (WIN_spec_mode == 2)
	    {
	      // log of the corrected freq
	      get_color (widget, 0, 255, 0, gc); // green
	      x = log (((double)i / (double)WIN_spec_n + r_dphi [i])
		       * sfinfo.samplerate);
	      ix0 = logf_to_display (x, WIN_wav_width);
	      gdk_draw_line (wav_pixmap, gc,
			     ix0, bottom_spec,
			     ix0, bottom_spec - iy);
	    }

	  // right phase
	  get_color (widget, 0, 255, 0, gc); // green
	  iy = bottom_phase
	    - (int)((double)height_phase
		    * (r_ph [i] - ph_min) / (ph_max - ph_min));
	  rad = (int)((double)max_rad_phase
		      * (y - amp2_min) / (amp2_max - amp2_min));
	  if (rad <= 0) rad = 1;
	  gdk_draw_arc (wav_pixmap, gc, TRUE, // fill
			ix-rad/2, iy-rad/2,
			rad, rad,
			0, 64*360);
	}

      /*
      // average bar
      if (WIN_spec_mode == 2)
	{
	  int ix1;
	  for (i = 0; i < resolution; i ++)
	    {
	      // set ix0 and ix1 by the middle point to the neighbors
	      ix0 = (int)(((double)i-0.5)
		* (double)WIN_wav_width / (double)resolution);
	      ix1 = (int)(((double)i+0.5)
		* (double)WIN_wav_width / (double)resolution);
	      if (ix0 < 0) ix0 = 0;
	      if (ix1 > WIN_wav_width) ix1 = WIN_wav_width;

	      // left channel
	      get_color (widget, 255, 0, 255, gc);

	      y = 2.0 * log10 (l_ave [i]); // log of the squared
	      if (y > amp2_max) y = amp2_max;
	      if (y >= amp2_min)
		{
		  iy = (int)((double)height_spec * (y - amp2_min)
			     / (amp2_max - amp2_min));

		  gdk_draw_line (wav_pixmap, gc,
				 ix0, bottom_spec,
				 ix0, bottom_spec - iy);
		  gdk_draw_line (wav_pixmap, gc,
				 ix0, bottom_spec - iy,
				 ix1, bottom_spec - iy);
		  gdk_draw_line (wav_pixmap, gc,
				 ix1, bottom_spec - iy,
				 ix1, bottom_spec);
		}

	      // right channel
	      get_color (widget, 0, 255, 255, gc);

	      y = 2.0 * log10 (r_ave [i]); // log of the squared
	      if (y > amp2_max) y = amp2_max;
	      if (y >= amp2_min)
		{
		  iy = (int)((double)height_spec * (y - amp2_min)
			     / (amp2_max - amp2_min));

		  gdk_draw_line (wav_pixmap, gc,
				 ix0, bottom_spec,
				 ix0, bottom_spec - iy);
		  gdk_draw_line (wav_pixmap, gc,
				 ix0, bottom_spec - iy,
				 ix1, bottom_spec - iy);
		  gdk_draw_line (wav_pixmap, gc,
				 ix1, bottom_spec - iy,
				 ix1, bottom_spec);
		}
	    }
	}
      */

      free (l_amp2);
      free (r_amp2);
      free (l_ph);
      free (r_ph);
      free (l_dphi);
      free (r_dphi);

      if (WIN_spec_mode == 2)
	{
	  free (l_ave);
	  free (r_ave);
	}

      // recover GC's function
      gdk_gc_set_function (gc, backup_gc_values.function);
    }
  // the following is static contents


  // put scale (colormap) for power spectrum
  for (i = 0; i < height_spec; i ++)
    {
      y = amp2_min + ((double)i + .5) * (amp2_max - amp2_min) / height_spec;
      ic = (int)(256.0 * (y - amp2_min) / (amp2_max - amp2_min));
      if (ic < 0)    ic = 0;
      if (ic >= 256) ic = 255;
      get_color (widget,
		 colormap_power_r[ic],
		 colormap_power_g[ic],
		 colormap_power_b[ic],
		 gc);
      gdk_draw_line (wav_pixmap, gc,
		     0, bottom_spec - i,
		     9, bottom_spec - i);
    }

  // horizontal lines
  get_color (widget, 255, 255, 255, gc); // white
  // between spec and phase
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom_spec,
		 WIN_wav_width, bottom_spec);
  // bottom of the phase
  gdk_draw_line (wav_pixmap, gc,
		 0, bottom_phase,
		 WIN_wav_width, bottom_phase);
}

/* draw spectrogram panel
 * INPUT
 *  i0, i1 : the range to draw in the display (pixel)
 */
static void
draw_spectrogram_frame (GtkWidget *widget,
			GdkGC *gc,
			int bottom_spg, int height_spg,
			int i0, int i1)
{
  extern GdkPixmap *wav_pixmap;
  //extern gint WIN_wav_width;
  extern int WIN_spec_mode;
  extern double amp2_min, amp2_max;
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;

  int i, j;
  int k;

  double x;
  int ix, ix0;
  double y;
  int ic;


  if (i0 < 10) i0 = 10;
  // 10 pixel is the width of keyboard picture in the left
  if (i0 >= i1) return;


  // clear image first
  get_color (widget, 0, 0, 0, gc);
  gdk_draw_rectangle (wav_pixmap, gc, TRUE /* fill */,
		      i0, bottom_spg - height_spg, 
		      i1, height_spg);


  // backup GdkFunction
  GdkGCValues backup_gc_values;
  gdk_gc_get_values (gc, &backup_gc_values);


  // the following is the dynamic
  if (sf != NULL)
    {
      // allocate working area
      double *l_amp2 = NULL;
      double *l_ph = NULL;
      double *l_dphi = NULL;
      l_amp2 = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
      l_ph   = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
      l_dphi = (double *)malloc (sizeof (double) * ((WIN_spec_n/2)+1));
      CHECK_MALLOC (l_amp2, "draw_spectrogram_frame");
      CHECK_MALLOC (l_ph,   "draw_spectrogram_frame");
      CHECK_MALLOC (l_dphi, "draw_spectrogram_frame");

      extern gint colormap_power_r[256];
      extern gint colormap_power_g[256];
      extern gint colormap_power_b[256];

      int istep;
      istep = WIN_spec_n / WIN_wav_scale;
      if (istep <= 0) istep = 1;

      double *ave = NULL;
      if (WIN_spec_mode == 1 || WIN_spec_mode == 2)
	{
	  ave = (double *)malloc (sizeof (double) * height_spg);
	  CHECK_MALLOC (ave, "draw_spectrogram_frame");
	}

      for (i = i0/*0*/; i < i1/*WIN_wav_width*/; i+=istep)
	{
	  if (WIN_spec_mode == 0)
	    {
	      // get amp2 for the frame (WIN_wav_cur + i * WIN_wav_scale)
	      fft_one_frame (WIN_wav_cur + i * WIN_wav_scale,
			     l_amp2, NULL, NULL, NULL);

	      // drawing
	      ix0 = -1;
	      y = 0.0;
	      int ny = 0;
	      for (k = 0; k < (WIN_spec_n/2)+1; k ++) // full span
		{
		  x = log ((double)k / (double)WIN_spec_n * sfinfo.samplerate);
		  if (x < logf_min || x > logf_max)
		    continue;

		  ix = logf_to_display (x, height_spg);
		  if (ix != ix0)
		    {
		      if (ny > 0)
			{
			  y /= (double) ny;
			  y = log10 (y);
			  ic = (int)(256.0
				     * (y - amp2_min) / (amp2_max - amp2_min));
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
				       / (double)WIN_spec_n
				       * sfinfo.samplerate);
			      ix1 = logf_to_display (x, height_spg);
			      if (ix1 != ix0)
				{
				  fprintf (stderr, "something is wrong...\n");
				  exit (1);
				}

			      x = log ((double)(k-2)
				       / (double)WIN_spec_n
				       * sfinfo.samplerate);
			      ix1 = logf_to_display (x, height_spg);
			      // so that (ix1, ix0, ix) are the positions
			      // => draw the rectangle for
			      // (ix0-(ix0-ix1)/2,ix0+(ix-ix0)/2)=(ix_b, ix_t)
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
				  gdk_draw_line
				    (wav_pixmap, gc,
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
				  gdk_draw_point
				    (wav_pixmap, gc,
				     i, bottom_spg - ix0);
				}
			      else
				{
				  if (i + istep >= WIN_wav_width)
				    {
				      gdk_draw_line
					(wav_pixmap, gc,
					 i,             bottom_spg - ix0,
					 WIN_wav_width, bottom_spg - ix0);
				    }
				  else
				    {
				      gdk_draw_line
					(wav_pixmap, gc,
					 i,         bottom_spg - ix0,
					 i + istep, bottom_spg - ix0);
				    }
				}
			    }
			}
		      // for the next step
		      y = 0.0;
		      ny = 0;
		      ix0 = ix;
		    }
		  y += l_amp2 [k];
		  ny ++;
		}
	    }
	  else // WIN_spec_mode == 1 || WIN_spec_mode == 2
	    {
	      // get amp2 and dphi for the frame
	      // (WIN_wav_cur + i * WIN_wav_scale)
	      fft_two_frames (WIN_wav_cur + i * WIN_wav_scale, WIN_spec_hop,
			      l_amp2, NULL,
			      l_ph, NULL,
			      l_dphi, NULL);
	      average_PV_FFT (height_spg, l_amp2, l_dphi, ave);

	      if (WIN_spec_mode == 1 ||
		  (WIN_spec_mode == 2 && height_spg < (oct_max-oct_min)*12))
		{
		  for (k = 0; k < height_spg; k ++)
		    {
		      y = 2.0 * log10 (ave [k]);
		      ic = (int)(256.0 * (y - amp2_min)
				 / (amp2_max - amp2_min));
		      if (ic < 0)    ic = 0;
		      if (ic >= 256) ic = 255;
		      get_color (widget,
				 colormap_power_r[ic],
				 colormap_power_g[ic],
				 colormap_power_b[ic],
				 gc);

		      if (istep <= 1)
			{
			  gdk_draw_point (wav_pixmap, gc, i, bottom_spg - k);
			}
		      else
			{
			  if (i + istep >= WIN_wav_width)
			    {
			      gdk_draw_line (wav_pixmap, gc,
					     i,             bottom_spg - k,
					     WIN_wav_width, bottom_spg - k);
			    }
			  else
			    {
			      gdk_draw_line (wav_pixmap, gc,
					     i,         bottom_spg - k,
					     i + istep, bottom_spg - k);
			    }
			}
		    }
		}
	      else // WIN_spec_mode == 2 and height_spg > 4*12
		{
		  int midi0, midi;
		  midi0 = -1;
		  y = 0.0;
		  int ny = 0;
		  for (k = 0; k < height_spg; k ++)
		    {
		      // current midi note for display position k
		      midi = logf_to_midi (display_to_logf (k, height_spg));
		      if (midi != midi0)
			{
			  if (ny > 0)
			    {
			      // draw for midi0 (= midi - 1)
			      y /= (double) ny;
			      y = 2.0 * log10 (y);
			      ic = (int)(256.0
					 * (y - amp2_min)
					 / (amp2_max - amp2_min));
			      if (ic < 0)    ic = 0;
			      if (ic >= 256) ic = 255;
			      get_color (widget,
					 colormap_power_r[ic],
					 colormap_power_g[ic],
					 colormap_power_b[ic],
					 gc);
			      int ix_b, ix_t, ix_d;
			      ix_b = midi_to_display_bottom (midi-1, height_spg);
			      ix_t = midi_to_display_top (midi-1, height_spg);
			      ix_d = ix_t - ix_b + 1;
			      if (ix_b < 0) ix_b = 0;
			      if (ix_d < 1) ix_d = 1;
			      if (ix_t > height_spg) ix_t = height_spg;

			      if (istep <= 1)
				{
				  gdk_draw_line
				    (wav_pixmap, gc,
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
			  // for the next step
			  y = 0.0;
			  ny = 0;
			  midi0 = midi;
			}
		      y += ave [k];
		      ny ++;
		    }
		}
	    }
	}
      free (l_amp2);
      free (l_ph);
      free (l_dphi);

      if (WIN_spec_mode == 1 || WIN_spec_mode == 2)
	{
	  free (ave);
	}

      // recover GC's function
      gdk_gc_set_function (gc, backup_gc_values.function);
    }
  // the following is static contents


  // cref lines
  get_color (widget, 255, 255, 255, gc); // white
  // G2 (midi 43)
  ix = bottom_spg - logf_to_display (midi_to_logf (43), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // B2 (midi 47)
  ix = bottom_spg - logf_to_display (midi_to_logf (47), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // D3 (midi 50)
  ix = bottom_spg - logf_to_display (midi_to_logf (50), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // F3 (midi 53)
  ix = bottom_spg - logf_to_display (midi_to_logf (53), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // A3 (midi 57)
  ix = bottom_spg - logf_to_display (midi_to_logf (57), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);

  // E4 (midi 64)
  ix = bottom_spg - logf_to_display (midi_to_logf (64), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // G4 (midi 67)
  ix = bottom_spg - logf_to_display (midi_to_logf (67), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // B4 (midi 71)
  ix = bottom_spg - logf_to_display (midi_to_logf (71), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // D5 (midi 74)
  ix = bottom_spg - logf_to_display (midi_to_logf (74), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);
  // F5 (midi 77)
  ix = bottom_spg - logf_to_display (midi_to_logf (77), height_spg);
  gdk_draw_line (wav_pixmap, gc, 10, ix, WIN_wav_width, ix);

  // set GC wiht XOR function
  gdk_gc_set_function (gc, GDK_OR);
  get_color (widget, 128, 128, 128, gc);
  // i is midi octave number
  for (i = oct_min; i < oct_max; i ++)
    {
      for (j = 0; j < 12; j ++)
	{
	  ix = bottom_spg
	    - midi_to_display_bottom ((i+1)*12+j, height_spg);
	  gdk_draw_line (wav_pixmap, gc,
			 i0/*10*/, ix,
			 i1/*WIN_wav_width*/, ix);
	}
    }
  // last line between B and C
  i = oct_max;
  j = 0;
  ix = bottom_spg - midi_to_display_bottom ((i+1)*12+j, height_spg);
  gdk_draw_line (wav_pixmap, gc,
		 i0/*10*/, ix,
		 i1/*WIN_wav_width*/, ix);

  // recover GC's function
  gdk_gc_set_function (gc, backup_gc_values.function);


  // horizontal lines
  get_color (widget, 255, 255, 255, gc); // white
  // separate line at the top of wav == the bottom of spectrogram
  gdk_draw_line (wav_pixmap, gc,
		 i0/*0*/, bottom_spg,
		 i1/*WIN_wav_width*/, bottom_spg);

  draw_keyboard (widget, gc, bottom_spg, height_spg);
}


/*
 * INPUT
 *  flag_spec : set 1 to update spec panel (when mark is set or changed)
 *  flag_wav  : set 1 to update wav panel
 *  i0, i1    : the range to draw in the display (pixel)
 */
static void
update_win_wav (GtkWidget *widget,
		int flag_spec,
		int flag_wav,
		int flag_spg)
{
  extern gint WIN_wav_width;
  extern gint WIN_wav_height;

  extern int WIN_wav_cur;
  extern int WIN_wav_scale;
  extern int WIN_wav_mark;
  extern int WIN_spec_n;
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;

  //if (sf == NULL) return;

  // first, create a GC to draw on
  GdkGC *gc;
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


  // WAV window
  if (flag_wav != 0)
    {
      draw_wav_frame (widget, gc,
		      bottom_wav, height_wav,
		      0, WIN_wav_width);
    }

  // Spectrum window
  if (flag_spec != 0)
    {
      draw_spectrum_frame (widget, gc,
			   bottom_spec, height_spec,
			   bottom_phase, height_phase);
    }

  // spectrogram
  if (flag_spg != 0)
    {
      draw_spectrogram_frame (widget, gc,
			      bottom_spg, height_spg,
			      0, WIN_wav_width);
    }

  // take pixbuf at this point (without texts)
  if (wav_pixbuf != NULL)
    {
      gdk_pixbuf_unref (wav_pixbuf);
    }
  wav_pixbuf = gdk_pixbuf_get_from_drawable (NULL, wav_pixmap, NULL,
					     0, 0,
					     0, 0,
					     WIN_wav_width, WIN_wav_height);

  // check mark
  // set GC wiht XOR function
  //gdk_gc_set_function (gc, GDK_XOR);
  gdk_gc_set_function (gc, GDK_OR_REVERSE);

  get_color (widget, 128, 128, 128, gc);
  int j0, j1;
  j0 = (WIN_wav_mark              - WIN_wav_cur) / WIN_wav_scale;
  j1 = (WIN_wav_mark + WIN_spec_n - WIN_wav_cur) / WIN_wav_scale;
  if (j0 <= WIN_wav_width && j1 >= 0)
    {
      if (j0 < 0) j0 = 0;
      if (j1 > WIN_wav_width) j1 = WIN_wav_width;
      if ((j1-j0) == 0) j1 = j0 + 1;
      gdk_draw_rectangle (wav_pixmap, gc, TRUE, // fill
			  j0, WIN_wav_height - (height_wav+height_spg),
			  (j1-j0), height_wav+height_spg);
    }
  // recover GC's function
  gdk_gc_set_function (gc, backup_gc_values.function);


  draw_infos (widget, gc,
	      0, bottom_spec, // top and bottom of spec window
	      255, 255, 255,
	      1, 128, 128, 128, "DejaVu Sans Light 10");


  // finally, destroy GC
  g_object_unref (gc);

  gtk_widget_queue_draw_area (widget, 0, 0, WIN_wav_width, WIN_wav_height);
}


/* draw playing indicator
 */
void
draw_play_indicator (GtkWidget *widget)
{
  extern GdkPixmap *wav_pixmap;
  extern GdkPixbuf *wav_pixbuf;

  extern int flag_play;
  extern long play_cur;

  static long last = -1;

  if (flag_play == 0) return; // for sure


  // first, create a GC to draw on
  GdkGC *gc;
  gc = gdk_gc_new (widget->window);
  get_color (widget, 255, 0, 0, gc); // red

  // vertical axis is directing down on the window
  extern gint WIN_wav_height;
  int bottom_spg = WIN_wav_height*29/32;
  int height_spg = WIN_wav_height*5/8;

  int bottom_wav = WIN_wav_height;
  int height_wav = WIN_wav_height*3/32;


  int ix;
  ix  = (int)((play_cur  - WIN_wav_cur) / WIN_wav_scale);
  if (last < 0)
    {
      // no indicator before
      gdk_draw_line (wav_pixmap, gc,
		     ix, bottom_spg - height_spg,
		     ix, bottom_wav);
      gtk_widget_queue_draw_area (widget,
				  ix, bottom_spg - height_spg,
				  1,  height_spg + height_wav);
    }
  else
    {
      int ix0;
      ix0 = (int)((last - WIN_wav_cur) / WIN_wav_scale);
      if (ix != ix0)
	{
	  gdk_draw_line (wav_pixmap, gc,
			 ix, bottom_spg - height_spg,
			 ix, bottom_wav);

	  // erase the existing indicator
	  gdk_pixbuf_render_to_drawable (wav_pixbuf,
					 wav_pixmap, gc,
					 ix0, bottom_spg - height_spg,
					 ix0, bottom_spg - height_spg,
					 1,   height_spg + height_wav,
					 GDK_RGB_DITHER_NONE, 0, 0);

	  gtk_widget_queue_draw_area (widget,
				      ix, bottom_spg - height_spg,
				      1,  height_spg + height_wav);
	  gtk_widget_queue_draw_area (widget,
				      ix0, bottom_spg - height_spg,
				      1,   height_spg + height_wav);
	}
      // otherwise, indicator is in the same position (so do nothing)
    }

  last = play_cur; // frame

  // finally, destroy GC
  g_object_unref (gc);
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
  update_win_wav (widget,
		  1, // spec
		  1, // wav
		  1  // spg
		  );

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
  extern GdkPixmap *wav_pixmap;
  extern gint WIN_wav_width;
  extern int flag_window;
  extern double amp2_min, amp2_max;
  extern int oct_min, oct_max;
  extern double logf_min, logf_max;

  extern int flag_play;
  extern gint tag_play;

  extern struct pv_complex_data *pv;


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
      //fprintf (stdout, "Win_spec_n = %d\n", WIN_spec_n);
      free (spec_in);
      free (spec_out);
      fftw_destroy_plan (plan);
      spec_in  = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      spec_out = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      CHECK_MALLOC (spec_in,  "wav_key_press_event");
      CHECK_MALLOC (spec_out, "wav_key_press_event");
      plan = fftw_plan_r2r_1d (WIN_spec_n, spec_in, spec_out,
			       FFTW_R2HC, FFTW_ESTIMATE);
      spec_left  = (double *)realloc (spec_left, sizeof(double) * WIN_spec_n);
      spec_right = (double *)realloc (spec_right, sizeof(double) * WIN_spec_n);
      CHECK_MALLOC (spec_left,  "wav_key_press_event");
      CHECK_MALLOC (spec_right, "wav_key_press_event");

      WIN_spec_hop = WIN_spec_n / WIN_spec_hop_scale;

      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_Left:
      WIN_spec_n /= 2;
      if (WIN_spec_n < 16) WIN_spec_n = 16; // is it enough?
      //fprintf (stdout, "Win_spec_n = %d\n", WIN_spec_n);
      free (spec_in);
      free (spec_out);
      fftw_destroy_plan (plan);
      spec_in  = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      spec_out = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
      CHECK_MALLOC (spec_in,  "wav_key_press_event");
      CHECK_MALLOC (spec_out, "wav_key_press_event");
      plan = fftw_plan_r2r_1d (WIN_spec_n, spec_in, spec_out,
			       FFTW_R2HC, FFTW_ESTIMATE);
      spec_left  = (double *)realloc (spec_left, sizeof(double) * WIN_spec_n);
      spec_right = (double *)realloc (spec_right, sizeof(double) * WIN_spec_n);
      CHECK_MALLOC (spec_left,  "wav_key_press_event");
      CHECK_MALLOC (spec_right, "wav_key_press_event");

      WIN_spec_hop = WIN_spec_n / WIN_spec_hop_scale;

      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_h:
      WIN_spec_hop_scale /= 2;
      if (WIN_spec_hop_scale <= 1) WIN_spec_hop_scale = 1;
      WIN_spec_hop = WIN_spec_n / WIN_spec_hop_scale;
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_H:
      WIN_spec_hop_scale *= 2;
      WIN_spec_hop = WIN_spec_n / WIN_spec_hop_scale;
      if (WIN_spec_hop == 0)
	{
	  WIN_spec_hop = 1;
	  WIN_spec_hop_scale = WIN_spec_n / WIN_spec_hop;
	}
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_p:
      WIN_spec_mode ++;
      if (WIN_spec_mode > 2) WIN_spec_mode = 0;
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_P:
      WIN_spec_mode --;
      if (WIN_spec_mode < 0) WIN_spec_mode = 2;
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_W:
      flag_window --;
      if (flag_window < 0) flag_window = 6;
      //fprint_window_name (stdout, flag_window);
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;
    case GDK_w:
      flag_window ++;
      if (flag_window > 6) flag_window = 0;
      //fprint_window_name (stdout, flag_window);
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_o:
      oct_max ++;
      if (oct_max > 9) oct_max = 9;
      logf_max = midi_to_logf ((oct_max+1)*12);
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;
    case GDK_O:
      oct_max --;
      if (oct_max <= oct_min) oct_max = oct_min + 1; 
      logf_max = midi_to_logf ((oct_max+1)*12);
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_L:
      oct_min ++;
      if (oct_min >= oct_max) oct_min = oct_max - 1;
      logf_min = midi_to_logf ((oct_min+1)*12);
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;
    case GDK_l:
      oct_min --;
      if (oct_min <= -1) oct_min = -1;
      logf_min = midi_to_logf ((oct_min+1)*12);
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_Up:
      amp2_max += 1.0;
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;
    case GDK_Down:
      amp2_max -= 1.0;
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_Page_Up:
      amp2_min += 1.0;
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;
    case GDK_Page_Down:
      amp2_min -= 1.0;
      update_win_wav (widget,
		      1, // spec
		      0, // wav
		      1  // spg
		      );
      break;

    case GDK_space:
      flag_play ++;
      if (flag_play > 1)
	{
	  fprintf (stderr, "# STOP\n");
	  flag_play = 0;
	  // gtk_timeout_remove is deprecated
	  g_source_remove (tag_play);
	}
      else
	{
	  fprintf (stderr, "# PLAY\n");
	  // gtk_timeout_add is deprecated
	  tag_play = g_timeout_add (100, // miliseconds
				    play_1msec, widget);
	}
      break;

    case GDK_Q:
    case GDK_q:
      if (pv->flag_lock == 0)
	{
	  pv->flag_lock = 1;
	  fprintf (stderr, "# loose phase lock\n");
	}
      else
	{
	  pv->flag_lock = 0;
	  fprintf (stderr, "# no phase lock\n");
	}
      break;

    default:
      return FALSE; /* propogate event */ 
    }

  return TRUE;
}

static gint
wav_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  extern GdkPixmap *wav_pixmap;
  extern SF_INFO sfinfo;

  if (wav_pixmap == NULL)
    {
      return FALSE; /* propogate event */ 
    }

  switch (event->button)
    {
    case 1:
      //g_print ("Button Press %.0f, %.0f\n", event->x, event->y);
      WIN_wav_mark = WIN_wav_cur + WIN_wav_scale * event->x;
      if (WIN_wav_mark > (sfinfo.frames - WIN_spec_n))
	WIN_wav_mark = sfinfo.frames - WIN_spec_n;
      update_win_wav (widget,
		      1, // spec
		      1, // wav
		      1  // spg
		      );
      // NOTE ** we can set the limited range here to ease CPU!!
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
  extern GdkPixmap *wav_pixmap;
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
    
    /*
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
    */

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

  update_win_wav (GTK_WIDGET (set),
		  0, // spec
		  1, // wav
		  1  // spg
		  );
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

  GTK_ADJUSTMENT (adj_cur)->page_increment
    = (double)(WIN_wav_scale * WIN_wav_width);
  GTK_ADJUSTMENT (adj_cur)->page_size
    = (double)(WIN_wav_scale * WIN_wav_width);

  gtk_adjustment_changed (GTK_ADJUSTMENT (adj_cur));

  update_win_wav (GTK_WIDGET (set),
		  0, // spec
		  1, // wav
		  1  // spg
		  );
}

static void
wav_pv_rate (GtkAdjustment *get, GtkAdjustment *set)
{
  extern double pv_rate;

  pv_rate = get->value;

  //update_win_wav (GTK_WIDGET (set));
}

static void
wav_pv_pitch (GtkAdjustment *get, GtkAdjustment *set)
{
  extern double pv_pitch;

  pv_pitch = get->value;

  //update_win_wav (GTK_WIDGET (set));
}


static void
make_color_map (void)
{
  extern gint colormap_power_r[256];
  extern gint colormap_power_g[256];
  extern gint colormap_power_b[256];

  int i;
  int i0;
  for (i = 0; i < 256/5; i ++)
    {
      colormap_power_r[i] = 0;
      colormap_power_g[i] = 0;
      colormap_power_b[i] = i*5;
    }
  i0 = 256/5;
  for (i = 0; i < 256/5; i ++)
    {
      colormap_power_r[i0+i] = 0;
      colormap_power_g[i0+i] = i*5;
      colormap_power_b[i0+i] = 255;
    }
  i0 = 256*2/5;
  for (i = 0; i < 256/5; i ++)
    {
      colormap_power_r[i0+i] = 0;
      colormap_power_g[i0+i] = 255;
      colormap_power_b[i0+i] = 255-i*5;
    }
  i0 = 256*3/5;
  for (i = 0; i < 256/5; i ++)
    {
      colormap_power_r[i0+i] = i*5;
      colormap_power_g[i0+i] = 255;
      colormap_power_b[i0+i] = 0;
    }
  i0 = 256*4/5;
  for (i = 0; i < 256/5; i ++)
    {
      colormap_power_r[i0+i] = 255;
      colormap_power_g[i0+i] = 255-i*5;
      colormap_power_b[i0+i] = 0;
    }
  // for sure
  colormap_power_r[255] = 255;
  colormap_power_g[255] = 0;
  colormap_power_b[255] = 0;
}


static gboolean
wav_delete (GtkWidget *widget,
	    GdkEvent *event,
	    gpointer data)
{
  /* If you return FALSE in the "delete_event" signal handler,
   * GTK will emit the "destroy" signal. Returning TRUE means
   * you don't want the window to be destroyed.
   * This is useful for popping up 'are you sure you want to quit?'
   * type dialogs. */

  // sndfile device
  extern SNDFILE *sf;
  sf_close (sf);

  // ao device
  extern ao_device *ao;
  ao_close (ao);


  return FALSE;
}

/** **/
void
create_wav (void)
{
  extern SNDFILE *sf;
  extern SF_INFO sfinfo;

  extern GtkObject * adj_cur;
  extern GtkObject * adj_scale;

  // init params
  make_color_map ();

  extern int WIN_spec_n;
  WIN_spec_n = 2048;
  extern int WIN_spec_hop_scale;
  extern int WIN_spec_hop;
  WIN_spec_hop_scale = 4;
  WIN_spec_hop = WIN_spec_n / WIN_spec_hop_scale;
  extern int WIN_spec_mode;
  WIN_spec_mode = 0;

  extern struct pv_complex_data *pv;
  pv = pv_complex_init (WIN_spec_n, WIN_spec_hop, 3 /* hanning */);
  pv_complex_set_input (pv, sf, &sfinfo);
  //pv->pitch_shift = 0.0;


  extern double *spec_in;
  extern double *spec_out;
  extern fftw_plan plan;
  spec_in  = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
  spec_out = (double *)fftw_malloc (sizeof(double) * WIN_spec_n);
  CHECK_MALLOC (spec_in,  "wav_key_press_event");
  CHECK_MALLOC (spec_out, "wav_key_press_event");
  plan = fftw_plan_r2r_1d (WIN_spec_n, spec_in, spec_out,
			   FFTW_R2HC, FFTW_ESTIMATE);

  extern int flag_window;
  flag_window = 0; // no window
  extern double amp2_min, amp2_max;
  amp2_min = -3.0;
  amp2_max = 1.0;

  extern double *spec_left;
  extern double *spec_right;
  spec_left  = (double *)malloc (sizeof(double) * WIN_spec_n);
  spec_right = (double *)malloc (sizeof(double) * WIN_spec_n);
  CHECK_MALLOC (spec_left,  "wav_key_press_event");
  CHECK_MALLOC (spec_right, "wav_key_press_event");

  extern int WIN_wav_cur;
  extern int WIN_wav_scale;
  extern int WIN_wav_mark;
  WIN_wav_cur = 0;
  WIN_wav_mark = 0;
  if (sfinfo.frames > WIN_spec_n)
    {
      WIN_wav_scale = WIN_spec_n;
    }
  else
    {
      WIN_wav_scale = sfinfo.frames;
    }

  extern int oct_min, oct_max;
  extern double logf_min, logf_max;
  oct_min = 2; // C2 is the lowest note
  oct_max = 6; // C6 is the highest note (precisely, B5 is)
  logf_min = midi_to_logf ((oct_min+1)*12);
  logf_max = midi_to_logf ((oct_max+1)*12);


  // ao device
  fprintf (stderr, "# check ao device\n");
  ao_initialize ();
  print_ao_driver_info_list ();
  ao_shutdown ();

  extern ao_device *ao;
  ao = ao_init_16_stereo (sfinfo.samplerate);
  pv_complex_set_output_ao (pv, ao);


  extern long play_cur;
  extern int flag_play;
  play_cur = 0;
  flag_play = 0;


  // make main window
  extern gint WIN_wav_width;
  extern gint WIN_wav_height;
  WIN_wav_width = 800;
  WIN_wav_height = 600;

  GtkWidget *window;
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "gWaoN");
  gtk_widget_set_size_request (GTK_WIDGET (window),
			       WIN_wav_width, WIN_wav_height);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroy),
		      GTK_OBJECT (window));
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		      GTK_SIGNAL_FUNC (wav_delete), NULL);


  GtkWidget *hbox;
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_widget_show (hbox);


  GtkWidget *vbox;
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
  //gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);


  // WAV window
  GtkWidget *wav_win;
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
  // Signals used to handle backing pixmap
  gtk_signal_connect (GTK_OBJECT (wav_win), "expose_event",
                      (GtkSignalFunc) wav_expose_event, NULL);
  gtk_signal_connect (GTK_OBJECT(wav_win),"configure_event",
                      (GtkSignalFunc) wav_configure_event, NULL);
  // Event signals
  gtk_signal_connect (GTK_OBJECT (wav_win), "motion_notify_event",
                      (GtkSignalFunc) wav_motion_notify_event, NULL);
  gtk_signal_connect (GTK_OBJECT (wav_win), "key_press_event",
                      (GtkSignalFunc) wav_key_press_event, NULL);
  gtk_signal_connect (GTK_OBJECT (wav_win), "button_press_event",
                      (GtkSignalFunc) wav_button_press_event, NULL);


  // add scrollbar
  adj_cur = gtk_adjustment_new
    ((double) WIN_wav_cur,
     0.0, // lower
     (double)sfinfo.frames, // upper
     1.0, // step increment
     (double)(WIN_wav_scale * WIN_wav_width), // page increment
     (double)(WIN_wav_scale * WIN_wav_width)); // page size
  g_signal_connect (G_OBJECT (adj_cur), "value_changed",
		    G_CALLBACK (wav_adj_cur), GTK_OBJECT (wav_win));

  GtkWidget *scrollbar;
  scrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (adj_cur));
  gtk_range_set_update_policy (GTK_RANGE (scrollbar),
			       GTK_UPDATE_CONTINUOUS);
  gtk_box_pack_start (GTK_BOX (vbox), scrollbar, FALSE, FALSE, 0);
  gtk_widget_show (scrollbar);


  // add scale
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

  GtkWidget *scale;
  scale = gtk_hscale_new (GTK_ADJUSTMENT (adj_scale));
  gtk_range_set_update_policy (GTK_RANGE (scale),
			       GTK_UPDATE_CONTINUOUS);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);



  // add scale for time-stretch
  extern double pv_rate;
  pv_rate  = 1.0; // initial value
  GtkObject *adj_pv_rate;
  adj_pv_rate = gtk_adjustment_new
    (1.0, // value
     -2.5, // lower
     2.5, // upper
     0.01, // step increment
     0.1, // page increment
     0.1); // page size
  g_signal_connect (G_OBJECT (adj_pv_rate), "value_changed",
		    G_CALLBACK (wav_pv_rate), GTK_OBJECT (wav_win));

  GtkWidget *scale_rate;
  scale_rate = gtk_vscale_new (GTK_ADJUSTMENT (adj_pv_rate));
  gtk_range_set_update_policy (GTK_RANGE (scale_rate),
			       GTK_UPDATE_CONTINUOUS);
  gtk_box_pack_start (GTK_BOX (hbox), scale_rate, FALSE, FALSE, 0);
  gtk_scale_set_digits (GTK_SCALE (scale_rate), 2);
  gtk_range_set_inverted (GTK_RANGE (scale_rate), TRUE);
  gtk_widget_show (scale_rate);


  // add scale for pitch-shift
  extern double pv_pitch;
  pv_pitch = 1.0; // initial value
  GtkObject *adj_pv_pitch;
  adj_pv_pitch = gtk_adjustment_new
    (0.0, // value
     -12.5, // lower
     12.5, // upper
     0.1, // step increment
     0.5, // page increment
     0.5); // page size
  g_signal_connect (G_OBJECT (adj_pv_pitch), "value_changed",
		    G_CALLBACK (wav_pv_pitch), GTK_OBJECT (wav_win));

  GtkWidget *scale_pitch;
  scale_pitch = gtk_vscale_new (GTK_ADJUSTMENT (adj_pv_pitch));
  gtk_range_set_update_policy (GTK_RANGE (scale_pitch),
			       GTK_UPDATE_CONTINUOUS);
  gtk_box_pack_start (GTK_BOX (hbox), scale_pitch, FALSE, FALSE, 0);
  gtk_range_set_inverted (GTK_RANGE (scale_pitch), TRUE);
  gtk_widget_show (scale_pitch);


  // everything is done
  gtk_widget_show (window);
}
