/* some wrapper for ao
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: ao-wrapper.c,v 1.3 2007/03/11 01:39:41 kichiki Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <stdlib.h>
#include <unistd.h> // write()

#include "memory-check.h" // CHECK_MALLOC() macro

// ao device
#include <ao/ao.h>


static void
print_ao_info (const ao_info *info,
	       const char *label)
{
  fprintf (stdout, "%s type : %d\n", label, info->type);
  fprintf (stdout, "%s name : %s\n", label, info->name);
  fprintf (stdout, "%s short name : %s\n", label, info->short_name);
  fprintf (stdout, "%s driver comment : %s\n", label, info->comment);
}


/* open ao device
 * the mode is 16-bits, stereo.
 */
ao_device *
ao_init_16_stereo (int samplerate)
{
  ao_device *device;
  ao_sample_format format;
  int default_driver;

  ao_initialize ();

  default_driver = ao_default_driver_id ();

  format.bits = 16;
  format.channels = 2;
  format.rate = samplerate;
  format.byte_format = AO_FMT_LITTLE;

  device = ao_open_live (default_driver, &format, NULL /* no options */);
  if (device == NULL) {
    fprintf(stderr, "Error opening device.\n");
    return NULL;
  }   

  // see the driver info
  ao_info *info;
  info = ao_driver_info (default_driver);
  print_ao_info (info, "[ao]");

  return (device);
}

void
print_ao_driver_info_list (void)
{
  int driver_count;
  ao_info** info_list;

  info_list = ao_driver_info_list (&driver_count);

  int i;
  char label [80];
  for (i = 0; i < driver_count; i ++)
    {
      sprintf (label, "%d : ", i);
      print_ao_info (info_list [i], label);
    }
}


int
ao_write (ao_device *device, double *left, double *right, int len)
{
  int i;

  int status;

  char *buffer = NULL;
  buffer = (char *)malloc (len * 4 * sizeof(char));
  CHECK_MALLOC (buffer, "ao_write");

  short sl;
  short sr;
  for (i = 0; i < len; i ++)
    {
      sl = (short)(left  [i] * 32768.0);
      sr = (short)(right [i] * 32768.0);
      buffer [i*4+0] =  sl       & 0xff;
      buffer [i*4+1] = (sl >> 8) & 0xff;
      buffer [i*4+2] =  sr       & 0xff;
      buffer [i*4+3] = (sr >> 8) & 0xff;
    }

  status = ao_play (device, buffer, len * 4);
  if (status != 0)
    {
      status = len * 4;
    }

  free (buffer);

  return (status);
}
