/* header file for notes.c --
 * routines for notes handling
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: notes.h,v 1.1 2007/11/04 23:49:55 kichiki Exp $
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
#ifndef	_NOTES_H_
#define	_NOTES_H_


struct WAON_notes {
  int n;       // number of events
  int  *step;  // step for the events
  char *event; // event type (0 == off, 1 == on)
  char *note;  // midi note number (0-127)
  char *vel;   // velocity of the note (for on) (0-127)
};


struct WAON_notes *
WAON_notes_init (void);

void
WAON_notes_free (struct WAON_notes *notes);

void
WAON_notes_append (struct WAON_notes *notes,
		   int step, char event, char note, char vel);


/** for stage 3 : time-difference check for note-on/off **/
/* check on and off events for each note comparing vel[] and on_vel[]
 * INPUT
 *  step           : the present step
 *  vel[128]       : velocity at the present step
 *  on_event[128]  : index of notes at the last on-event for each note.
 *                   (-1) means the note is off at the last step.
 *  on_threshold   : note turns on if vel[i] > on_threshold.
 *  off_threshold  : note turns off if vel[i] <= off_threshold.
 *  peak_threshold : note turns off and on
 *                   if vel[i] >= (on_vel[i] + peak_threshold)
 * OUTPUT
 *  notes : struct WAON_notes. event(s) are appended if happens.
 */
void
WAON_notes_check (struct WAON_notes *notes,
		  int step, char *vel, int *on_step,
		  int on_threshold,
		  int off_threshold,
		  int peak_threshold);


#endif /* !_NOTES_H_ */
