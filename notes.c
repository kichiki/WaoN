/* routines for notes handling
 * Copyright (C) 2007,2011 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: notes.c,v 1.2 2011/12/27 13:00:20 kichiki Exp $
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

void
WAON_notes_insert (struct WAON_notes *notes,
		   int index,
		   int step, char event, char note, char vel)
{
  notes->n ++;
  notes->step  = (int  *)realloc (notes->step,  sizeof (int)  * (notes->n));
  notes->event = (char *)realloc (notes->event, sizeof (char) * (notes->n));
  notes->note  = (char *)realloc (notes->note,  sizeof (char) * (notes->n));
  notes->vel   = (char *)realloc (notes->vel,   sizeof (char) * (notes->n));
  CHECK_MALLOC (notes->step,  "WAON_notes_insert");
  CHECK_MALLOC (notes->event, "WAON_notes_insert");
  CHECK_MALLOC (notes->note,  "WAON_notes_insert");
  CHECK_MALLOC (notes->vel,   "WAON_notes_insert");

  // copy elements (index, ..., n-2) into (index+1, ..., n-1), where
  // n is incremented n
  int i;
  for (i = notes->n - 1; i > index; i --)
    {
      notes->step [i] = notes->step [i - 1];
      notes->event[i] = notes->event[i - 1];
      notes->note [i] = notes->note [i - 1];
      notes->vel  [i] = notes->vel  [i - 1];
    }

  // set data at index
  notes->step [index] = step;
  notes->event[index] = event;
  notes->note [index] = note;
  notes->vel  [index] = vel;
}

void
WAON_notes_remove_at (struct WAON_notes *notes,
		      int index)
{
  // copy elements (index+1, ..., n-1) into (index, ..., n-2)
  int i;
  for (i = index + 1; i < notes->n; i ++)
    {
      notes->step [i - 1] = notes->step [i];
      notes->event[i - 1] = notes->event[i];
      notes->note [i - 1] = notes->note [i];
      notes->vel  [i - 1] = notes->vel  [i];
    }

  notes->n --;

  // resize the arrays
  notes->step  = (int  *)realloc (notes->step,  sizeof (int)  * (notes->n));
  notes->event = (char *)realloc (notes->event, sizeof (char) * (notes->n));
  notes->note  = (char *)realloc (notes->note,  sizeof (char) * (notes->n));
  notes->vel   = (char *)realloc (notes->vel,   sizeof (char) * (notes->n));
  CHECK_MALLOC (notes->step,  "WAON_notes_remove_at");
  CHECK_MALLOC (notes->event, "WAON_notes_remove_at");
  CHECK_MALLOC (notes->note,  "WAON_notes_remove_at");
  CHECK_MALLOC (notes->vel,   "WAON_notes_remove_at");
}

// shift indices in on_index[] larger than i_rm
// where i_rm is the removed index
void
check_on_index_for_remove (int* on_index, int i_rm)
{
  int i;
  for (i = 0; i < 128; i ++)
    {
      if (on_index[i] > i_rm)
	{
	  on_index[i] --;
	}
    }
}
void
WAON_notes_regulate (struct WAON_notes *notes)
{
  int *on_step  = (int *)malloc (sizeof (int) * 128);
  int *on_index = (int *)malloc (sizeof (int) * 128);
  CHECK_MALLOC (on_step,  "WAON_notes_remove_regulate");
  CHECK_MALLOC (on_index, "WAON_notes_remove_regulate");

  int i;
  for (i = 0; i < 128; i ++)
    {
      on_step [i] = -1;
      on_index[i] = -1;
    }

  int index;
  for (index = 0; index < notes->n; index ++)
    {
      int note = (int)notes->note[index];


      if (notes->event[index] == 0)
	{
	  // off event
	  if (on_step[note] < 0 || on_index[note] < 0)
	    {
	      // no on event on the note
	      // so remove this orphant off event
	      WAON_notes_remove_at (notes, index);
	      index --;
	    }

	  // reset on_step[] and on_index[]
	  on_step [note] = -1;
	  on_index[note] = -1;
	}
      else if (notes->event[index] == 1)
	{
	  // on event
	  if (on_step[note] >= 0 && on_index[note] >= 0)
	    {
	      // the note is already on
	      // so, insert off event here
	      WAON_notes_insert (notes, index,
				 notes->step[index],
				 0,    // off
				 note,
				 64);  // default
	      index ++;
	    }

	  // set on_step[] and on_index[]
	  on_step [note] = notes->step[index];
	  on_index[note] = index;
	}
      else
	{
	  fprintf (stderr, "# error: invalid event type %d\n",
		   notes->event[index]);
	}
    }

  // check if on note left
  int last_step = notes->step[notes->n - 1];
  for (i = 0; i < 128; i ++)
    {
      if (on_step[i] < 0) continue;

      WAON_notes_append (notes,
			 last_step + 1,
			 0, // off event
			 (char)i,
			 64);
    }

  free (on_step);
  free (on_index);
}

void
WAON_notes_remove_shortnotes (struct WAON_notes *notes,
			      int min_duration,
			      int min_vel)
{
  int *on_step  = (int *)malloc (sizeof (int) * 128);
  int *on_index = (int *)malloc (sizeof (int) * 128);
  CHECK_MALLOC (on_step,  "WAON_notes_remove_shortnotes");
  CHECK_MALLOC (on_index, "WAON_notes_remove_shortnotes");

  int i;
  for (i = 0; i < 128; i ++)
    {
      on_step [i] = -1;
      on_index[i] = -1;
    }

  int index;
  for (index = 0; index < notes->n; index ++)
    {
      int note = (int)notes->note[index];


      if (notes->event[index] == 0)
	{
	  // off event
	  if (on_step[note] < 0 || on_index[note] < 0)
	    {
	      // no on event on the note
	      // so remove this orphant off event
	      WAON_notes_remove_at (notes, index);
	      index --;
	    }
	  else
	    {
	      int vel = (int)notes->vel[on_index[note]];
	      int duration = notes->step[index] - on_step[note];
	      if (duration <= min_duration && vel <= min_vel)
		{
		  // remove these on and off events on the note
		  WAON_notes_remove_at (notes, index);
		  index --;

		  int index_on = on_index[note];
		  WAON_notes_remove_at (notes, index_on);
		  index --;

		  // need to shift indices on on_index[]
		  check_on_index_for_remove (on_index, index_on);
		}
	    }

	  // reset on_step[] and on_index[]
	  on_step [note] = -1;
	  on_index[note] = -1;
	}
      else if (notes->event[index] == 1)
	{
	  // on event
	  if (on_step[note] >= 0 && on_index[note] >= 0)
	    {
	      // the note is already on
	      // so, insert off event here
	      WAON_notes_insert (notes, index,
				 notes->step[index],
				 0,    // off
				 note,
				 64);  // default
	      index ++;
	    }

	  // set on_step[] and on_index[]
	  on_step [note] = notes->step[index];
	  on_index[note] = index;
	}
      else
	{
	  fprintf (stderr, "# error: invalid event type %d\n",
		   notes->event[index]);
	}
    }


  free (on_step);
  free (on_index);
}

void
WAON_notes_remove_longnotes (struct WAON_notes *notes,
			     int max_duration,
			     int min_vel)
{
  int *on_step  = (int *)malloc (sizeof (int) * 128);
  int *on_index = (int *)malloc (sizeof (int) * 128);
  CHECK_MALLOC (on_step,  "WAON_notes_remove_longnotes");
  CHECK_MALLOC (on_index, "WAON_notes_remove_longnotes");

  int i;
  for (i = 0; i < 128; i ++)
    {
      on_step [i] = -1;
      on_index[i] = -1;
    }

  int index;
  for (index = 0; index < notes->n; index ++)
    {
      int note = (int)notes->note[index];


      if (notes->event[index] == 0)
	{
	  // off event
	  if (on_step[note] < 0 || on_index[note] < 0)
	    {
	      // no on event on the note
	      // so remove this orphant off event
	      WAON_notes_remove_at (notes, index);
	      index --;
	    }
	  else
	    {
	      int vel = (int)notes->vel[on_index[note]];
	      int duration = notes->step[index] - on_step[note];
	      if (duration >= max_duration && vel <= min_vel)
		{
		  // remove these on and off events on the note
		  WAON_notes_remove_at (notes, index);
		  index --;

		  int index_on = on_index[note];
		  WAON_notes_remove_at (notes, index_on);
		  index --;

		  // need to shift indices on on_index[]
		  check_on_index_for_remove (on_index, index_on);
		}
	    }

	  // reset on_step[] and on_index[]
	  on_step[note] = -1;
	  on_index[note] = -1;
	}
      else if (notes->event[index] == 1)
	{
	  // on event
	  if (on_step[note] >= 0 && on_index[note] >= 0)
	    {
	      // the note is already on
	      // so, insert off event here
	      WAON_notes_insert (notes, index,
				 notes->step[index],
				 0,    // off
				 note,
				 64);  // default
	      index ++;
	    }

	  // set on_step[] and on_index[]
	  on_step[note] = notes->step[index];
	  on_index[note] = index;
	}
    }


  free (on_step);
  free (on_index);
}

void
WAON_notes_remove_smallnotes (struct WAON_notes *notes,
			      int min_vel)
{
  int *on_step  = (int *)malloc (sizeof (int) * 128);
  int *on_index = (int *)malloc (sizeof (int) * 128);
  CHECK_MALLOC (on_step,  "WAON_notes_remove_smallnotes");
  CHECK_MALLOC (on_index, "WAON_notes_remove_smallnotes");

  int i;
  for (i = 0; i < 128; i ++)
    {
      on_step [i] = -1;
      on_index[i] = -1;
    }

  int index;
  for (index = 0; index < notes->n; index ++)
    {
      int note = (int)notes->note[index];


      if (notes->event[index] == 0)
	{
	  // off event
	  if (on_step[note] < 0 || on_index[note] < 0)
	    {
	      // no on event on the note
	      // so remove this orphant off event
	      WAON_notes_remove_at (notes, index);
	      index --;
	    }
	  else
	    {
	      int vel = (int)notes->vel[on_index[note]];
	      if (vel <= min_vel)
		{
		  // remove these on and off events on the note
		  WAON_notes_remove_at (notes, index);
		  index --;

		  int index_on = on_index[note];
		  WAON_notes_remove_at (notes, index_on);
		  index --;

		  // need to shift indices on on_index[]
		  check_on_index_for_remove (on_index, index_on);
		}
	    }

	  // reset on_step[] and on_index[]
	  on_step [note] = -1;
	  on_index[note] = -1;
	}
      else if (notes->event[index] == 1)
	{
	  // on event
	  if (on_step[note] >= 0 && on_index[note] >= 0)
	    {
	      // the note is already on
	      // so, insert off event here
	      WAON_notes_insert (notes, index,
				 notes->step[index],
				 0,    // off
				 note,
				 64);  // default
	      index ++;
	    }

	  // set on_step[] and on_index[]
	  on_step[note] = notes->step[index];
	  on_index[note] = index;
	}
    }


  free (on_step);
  free (on_index);
}

void
WAON_notes_remove_octaves (struct WAON_notes *notes)
{
  int *on_step  = (int *)malloc (sizeof (int) * 128);
  int *on_index = (int *)malloc (sizeof (int) * 128);
  int *flag_remove = (int *)malloc (sizeof (int) * 128);
  CHECK_MALLOC (on_step,  "WAON_notes_remove_octaves");
  CHECK_MALLOC (on_index, "WAON_notes_remove_octaves");
  CHECK_MALLOC (flag_remove, "WAON_notes_remove_octaves");

  int i;
  for (i = 0; i < 128; i ++)
    {
      on_step [i] = -1;
      on_index[i] = -1;
      flag_remove[i] = 0; // false
    }

  int index;
  for (index = 0; index < notes->n; index ++)
    {
      int note = (int)notes->note[index];


      if (notes->event[index] == 0)
	{
	  // off event
	  if (on_step[note] < 0 || on_index[note] < 0)
	    {
	      // no on event on the note
	      // so remove this orphant off event
	      WAON_notes_remove_at (notes, index);
	      index --;
	    }
	  else
	    {
	      if (flag_remove[note] > 0)
		{
		  // remove these on and off events on the note
		  WAON_notes_remove_at (notes, index);
		  index --;

		  int index_on = on_index[note];
		  WAON_notes_remove_at (notes, index_on);
		  index --;

		  // need to shift indices on on_index[]
		  check_on_index_for_remove (on_index, index_on);
		}
	    }

	  // reset on_step[] and on_index[]
	  on_step [note] = -1;
	  on_index[note] = -1;
	}
      else if (notes->event[index] == 1)
	{
	  // on event
	  if (on_step[note] >= 0 && on_index[note] >= 0)
	    {
	      // the note is already on
	      // so, insert off event here
	      WAON_notes_insert (notes, index,
				 notes->step[index],
				 0,    // off
				 note,
				 64);  // default
	      index ++;
	    }

	  // set on_step[] and on_index[]
	  on_step[note] = notes->step[index];
	  on_index[note] = index;

	  flag_remove[note] = 0; // false

	  int note_down = note - 12;
	  if (note_down < 0) continue;

	  if (on_step[note_down] >= 0 && on_index[note_down] >= 0)
	    {
	      if (notes->vel[index] < notes->vel[on_index[note_down]])
		{
		  flag_remove[note] = 1; // true
		}
	    }
	}
    }


  free (on_step);
  free (on_index);
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

void
WAON_notes_dump (struct WAON_notes *notes)
{
  int last_step = -1;
  int i;
  for (i = 0; i < notes->n; i ++)
    {
      if (notes->step[i] > last_step)
	{
	  fprintf (stdout, "%5d : ", notes->step[i]);
	  last_step = notes->step[i];
	}
      else
	{
	  fprintf (stdout, "      : ");
	}

      if (notes->event[i] == 0)
	{
	  fprintf (stdout, "off ");
	}
      else
	{
	  fprintf (stdout, "on  ");
	}

      fprintf (stdout, "%3d %3d\n", notes->note[i], notes->vel[i]);
    }
}
void
WAON_notes_dump2 (struct WAON_notes *notes)
{
  int *on_step  = (int *)malloc (sizeof (int) * 128);
  int *on_index = (int *)malloc (sizeof (int) * 128);
  CHECK_MALLOC (on_step,  "WAON_notes_dump2");
  CHECK_MALLOC (on_index, "WAON_notes_dump2");

  int i;
  for (i = 0; i < 128; i ++)
    {
      on_step [i] = -1;
      on_index[i] = -1;
    }


  for (i = 0; i < notes->n; i ++)
    {
      int note = (int)notes->note[i];

      if (notes->event[i] == 0)
	{
	  // off event
	  if (on_step[note] < 0 || on_index[note] < 0)
	    {
	      // orphant off event
	    }
	  else
	    {
	      int step = notes->step[on_index[note]];
	      int duration = notes->step[i] - on_step[note];
	      int vel = (int)notes->vel[on_index[note]];
	      fprintf (stdout,
		       "%5d : note %3d, duration %3d, vel %3d\n",
		       step,
		       note,
		       duration,
		       vel);
	    }
	  // reset on_step[]
	  on_step [note] = -1;
	  on_index[note] = -1;
	}
      else if (notes->event[i] == 1)
	{
	  // on event
	  if (on_step[note] >= 0 && on_index[note] >= 0)
	    {
	      // the note is already on
	      int step = notes->step[on_index[note]];
	      int duration = notes->step[i] - on_step[note];
	      int vel = (int)notes->vel[on_index[note]];
	      fprintf (stdout,
		       "%5d : note %3d, duration %3d, vel %3d (* no-off)\n",
		       step,
		       note,
		       duration,
		       vel);
	    }

	  on_step [note] = notes->step[i];
	  on_index[note] = i;
	}
    }

  // check if on note left
  int last_step = notes->step[notes->n - 1];
  for (i = 0; i < 128; i ++)
    {
      if (on_step[i] < 0) continue;

      int step = notes->step[on_index[i]];
      int duration = last_step + 1 - on_step[i];
      int vel = (int)notes->vel[on_index[i]];
      fprintf (stdout,
	       "%5d : note %3d, duration %3d, vel %3d (* no-off at the end)\n",
	       step,
	       i,
	       duration,
	       vel);
    }

  free (on_step);
  free (on_index);
}

