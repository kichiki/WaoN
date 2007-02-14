/* some wrapper for esound
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: esd-wrapper.c,v 1.2 2007/02/14 04:01:43 kichiki Exp $
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


// esd sound device
#include <esd.h>


long esd_write (int esd, double *left, double *right, int len)
{
  int i;

  int status;
  unsigned char * obuf = NULL;
  short sl;
  short sr;

  obuf = (unsigned char *) malloc (sizeof (unsigned char) * (len * 2* 2));

  for (i = 0; i < len; i ++)
    {
      sl = (short)(left  [i] * 32768.0);
      sr = (short)(right [i] * 32768.0);
      obuf [i*4+0] = (unsigned char)( sl       & 0xFF);
      obuf [i*4+1] = (unsigned char)((sl >> 8) & 0xFF);
      obuf [i*4+2] = (unsigned char)( sr       & 0xFF);
      obuf [i*4+3] = (unsigned char)((sr >> 8) & 0xFF);
    }

  status = write (esd, obuf, len * 4);

  free (obuf);
  return (status);
}
