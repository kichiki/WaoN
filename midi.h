/* header file for midi.c --
 * subroutines to write standard MIDI file
 * Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: midi.h,v 1.4 2007/02/11 23:37:33 kichiki Exp $
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
#ifndef	_MIDI_H_
#define	_MIDI_H_

#include "analyse.h" // struct ia_note


/* for estimate pitch shift  */
extern double adj_pitch;
extern double pitch_shift;
extern int n_pitch;

extern double mid2freq[128];


/** general midi-frequency stuff **/
double
midi_to_freq (int midi);
int
freq_to_midi (double f);
double
midi_to_logf (int midi);
int
logf_to_midi (double logf);

/* get std MIDI note from frequency
 * taken into account (global) adj_pitch
 * collecting information for (global) pitch_shift
 */
int get_note (double freq);

int smf_header_fmt (int fd,
		       unsigned short format,
		       unsigned short tracks,
		       unsigned short divisions);
int smf_prog_change (int fd, char channel, char prog);
int smf_tempo (int fd, unsigned long tempo);
int smf_note_on (int fd, long dtime, char note, char vel, char channel);
int smf_note_off (int fd, long dtime, char note, char vel, char channel);
int smf_track_head (int fd, unsigned long size);
int smf_track_end (int fd);
int write_var_len (int fd, long value);
int read_var_len (int fd, long *value);
int wblong (int fd, unsigned long ul);
int wbshort (int fd, unsigned short us);

/* MIDI output of data note_on_off[]
 * INPUT
 *  num : # of event (on/off)
 *  *note_on_off : struct of note signal
 *  div : divisioin
 *  filename : filename of output midi file
 */
void
output_midi (int nmidi, struct ia_note *notes, double div, char *filename);


#endif /* !_MIDI_H_ */
