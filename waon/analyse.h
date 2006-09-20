/*
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * $Id: analyse.h,v 1.1 2006/09/20 21:25:04 kichiki Exp $
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

void
note_intensity (int n, double p[], double cut_ratio, double rel_cut_ratio,
		int i0, int i1,
		double t0, char intens[]);
struct ia_note *
chk_note_on_off (int icnt, char i_lsts[], char * on_lst[],
		 struct ia_note *notes, int *num, int *nmidi);
void output_midi (int num, struct ia_note *notes, double div,
		  char *filename);

double patch_power (double freq_ratio);
void init_patch (char *file_patch, int plen, int nwin);

struct ia_note * init_ia_note (void);
struct ia_note * append_ia_note (struct ia_note *last);
struct ia_note * incr_ia_note (struct ia_note *last, int *num);
