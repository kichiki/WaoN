/* routines for notes handling
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: notes.c,v 1.1 2007/11/04 23:49:18 kichiki Exp $
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
#include <stdio.h> // fprintf()
#include <stdlib.h> // malloc()
#include <sys/errno.h> // errno
#include "memory-check.h" // CHECK_MALLOC() macro

#include "notes.h"



struct WAON_notes *
WAON_notes_init (void)
{
  struct WAON_notes *notes
    = (struct WAON_notes *)malloc (sizeof (struct WAON_notes));
  CHECK_MALLOC (notes, "WAON_notes_init");

  notes->n = 0;
  notes->step  = NULL;
  notes->event = NULL;
  notes->note  = NULL;
  notes->vel   = NULL;

  return (notes);
}

void
WAON_notes_free (struct WAON_notes *notes)
{
  if (notes == NULL) return;

  if (notes->step  != NULL) free (notes->step);
  if (notes->event != NULL) free (notes->event);
  if (notes->note  != NULL) free (notes->note);
  if (notes->vel   != NULL) free (notes->vel);
  free (notes);
}

void
WAON_notes_append (struct WAON_notes *notes,
		   int step, char event, char note, char vel)
{
  notes->n ++;
  notes->step  = (int  *)realloc (notes->step,  sizeof (int)  * (notes->n));
  notes->event = (char *)realloc (notes->event, sizeof (char) * (notes->n));
  notes->note  = (char *)realloc (notes->note,  sizeof (char) * (notes->n));
  notes->vel   = (char *)realloc (notes->vel,   sizeof (char) * (notes->n));
  CHECK_MALLOC (notes->step,  "WAON_notes_append");
  CHECK_MALLOC (notes->event, "WAON_notes_append");
  CHECK_MALLOC (notes->note,  "WAON_notes_append");
  CHECK_MALLOC (notes->vel,   "WAON_notes_append");

  int i = notes->n - 1; // the last element
  notes->step [i] = step;
  notes->event[i] = event;
  notes->note [i] = note;
  notes->vel  [i] = vel;
}


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
		  int step, char *vel, int *on_event,
		  int on_threshold,
		  int off_threshold,
		  int peak_threshold)
{
  /* loop for notes */
  int i;
  for (i = 0; i < 128; i++)
    {
      if (on_event[i] < 0) /* off at last step  */
	{
	  /* check the note-on event by on_threshold  */
	  if (vel[i] > on_threshold)
	    {
	      /* on  */
	      WAON_notes_append (notes,
				 step,
				 1, /* on */
				 (char)i, // midi note
				 vel[i]);
	      on_event[i] = notes->n - 1; // event index of notes.
	    }
	}
      else /* on at last step  */
	{
	  /* check the note-off event by off_threshold  */
	  if (vel[i] <= off_threshold)
	    {
	      /* off  */
	      WAON_notes_append (notes,
				 step,
				 0, /* off */
				 (char)i, // midi note
				 64);
	      on_event[i] = -1;
	    }
	  else /* now note is over off_threshold at least  */
	    {
	      if (vel[i] >= (notes->vel[on_event[i]] + peak_threshold))
		{
		  /* off  */
		  WAON_notes_append (notes,
				     step,
				     0, /* off */
				     (char)i, // midi note
				     64);
		  /* on  */
		  WAON_notes_append (notes,
				     step,
				     1, /* on */
				     (char)i, // midi note
				     vel[i]);
		  on_event[i] = notes->n - 1; // event index of notes.
		}
	      else if (vel[i] > notes->vel[on_event[i]])
		{
		  /* overwrite velocity  */
		  notes->vel[on_event[i]] = vel[i];
		}
	    }
	}
    }
}
