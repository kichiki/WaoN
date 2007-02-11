/* header file for analyse.c --
 * routines to analyse power spectrum and output notes
 * Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: analyse.h,v 1.4 2007/02/11 23:46:41 kichiki Exp $
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
#ifndef	_ANALYSE_H_
#define	_ANALYSE_H_

struct note_sig {
  int	step;		/* present time  */
  char	sig;		/* 0 - off / 1 - on  */
  char	note;		/* MIDI note # (0-127)  */
  char	intensity;	/* intensity of note (for on) (0-127)  */
};

/*#define BLOCK_SIZE 1024*/
#define BLOCK_SIZE 8

struct ia_note {
  struct note_sig note[BLOCK_SIZE];
  struct ia_note *next;
};

/* global variables  */
extern int abs_flg; /* flag for absolute/relative cutoff  */
extern int patch_flg; /* flag for using patch file  */
extern double *pat; /* work area for patch  */
extern int npat; /* # of data in pat[]  */
extern double p0; /* maximum power  */
extern double if0; /* freq point of maximum  */
extern int peak_threashold; /* to select peaks in a note  */


/** for stage 2 : note selection process **/

/* get intensity of notes from power spectrum
 * INPUT
 *  p[]              : power spectrum
 *  fp[]             : frequencies for each bin
 *                     if NULL, we use the center frequencies
 *  cut_ratio        : log10 of cutoff ratio to scale velocity
 *  rel_cut_ratio    : log10 of cutoff ratio relative to average
 *                     0 means cutoff is equal to average
 *  (global)abs_flg  : 0 for relative, 1 for absolute
 *  i0, i1           : considering frequency range
 *  (global)patch_flg: whether patch is used or not.
 * OUTPUT
 *  intens[128]      : intensity [0,128) for each midi note
 */
void
note_intensity (double *p, double *fp,
		double cut_ratio, double rel_cut_ratio,
		int i0, int i1,
		double t0, char *intens);
/*
 * INPUT
 *  amp2 [(len/2)+1] : power spectrum (amp^2)
 *  dphi [(len/2)+1] : PV freq correction factor defined by
 *                     (1/2pi hop)principal(phi - phi0 - Omega),
 *                     therefore, the corrected freq is
 *                     (k/len + dphi[k])*samplerate [Hz].
 *                     give NULL for plain FFT power spectrum.
 * OUTPUT
 *  ave2 [128] : averaged amp2 for each midi note
 */
void
average_FFT_into_midi (int len, double samplerate,
		       const double *amp2, const double *dphi,
		       double *ave2);

/* pickup notes and its power from a table of power for each midi note
 * INPUT
 *  amp2midi [128]  : amp^2 for each midi note
 *  cut_ratio       : log10 of cutoff ratio to scale velocity
 *  rel_cut_ratio   : log10 of cutoff ratio relative to average
 *                    0 means cutoff is equal to average
 *  i0, i1          : considering midi note range (NOT FREQUENCY INDEX!!)
 *  (global)abs_flg : 0 for relative, 1 for absolute
 * OUTPUT
 *  intens[]        : with 127 elements (# of notes)
 */
void
pickup_notes (double *amp2midi,
	      double cut_ratio, double rel_cut_ratio,
	      int i0, int i1,
	      char *intens);


/** for stage 3 : time-difference check for note-on/off **/

/* check note on and off comparing intens[] now and last times
 * INPUT
 *  icnt : counter (present time)
 *  i_lsts[128] : intensity matrix
 *  on_lst[] : on-note list at last step
 *  *notes : ia_note pointer
 * OUTPUT
 *  nmidi : # of midi event
 *  num : current pointer of ia_note within a segment
 * RETURN VALUE
 *  current ia_note pointer
 */
struct ia_note *
chk_note_on_off (int icnt, char i_lsts[], char * on_lst[],
		 struct ia_note *notes, int *num, int *nmidi);



double patch_power (double freq_ratio);
void init_patch (char *file_patch, int plen, int nwin);

struct ia_note * init_ia_note (void);
struct ia_note * append_ia_note (struct ia_note *last);
struct ia_note * incr_ia_note (struct ia_note *last, int *num);


#endif /* !_ANALYSE_H_ */
