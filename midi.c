/* subroutines to write standard MIDI file
 * Copyright (C) 1998-2006 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: midi.c,v 1.2 2006/09/22 05:07:34 kichiki Exp $
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

#include <stdio.h> /* fputc(), etc  */
#include <unistd.h> /* write()  */
#include <math.h> /* log()  */
#include "midi.h"

/* global variables  */
double adj_pitch;
double pitch_shift;
int n_pitch;

double mid2freq[128] = 
{
  /* C0 -  */
  8.175799, 8.661957, 9.177024, 9.722718, 10.300861, 10.913382,
  11.562326, 12.249857, 12.978272, 13.750000, 14.567618, 15.433853,
  /* C1 -  */
  16.351598, 17.323914, 18.354048, 19.445436, 20.601722, 21.826764,
  23.124651, 24.499715, 25.956544, 27.500000, 29.135235, 30.867706,
  /* C2 -  */
  32.703196, 34.647829, 36.708096, 38.890873, 41.203445, 43.653529,
  46.249303, 48.999429, 51.913087, 55.000000, 58.270470, 61.735413,
  /* C3 -  */
  65.406391, 69.295658, 73.416192, 77.781746, 82.406889, 87.307058,
  92.498606, 97.998859, 103.826174, 110.000000, 116.540940, 123.470825,
  /* C4 -  */
  130.812783, 138.591315, 146.832384, 155.563492, 164.813778, 174.614116,
  184.997211, 195.997718, 207.652349, 220.000000, 233.081881, 246.941651,
  /* C5 -  */
  261.625565, 277.182631, 293.664768, 311.126984, 329.627557, 349.228231,
  369.994423, 391.995436, 415.304698, 440.000000, 466.163762, 493.883301,
  /* C6 -  */
  523.251131, 554.365262, 587.329536, 622.253967, 659.255114, 698.456463,
  739.988845, 783.990872, 830.609395, 880.000000, 932.327523, 987.766603,
  /* C7 -  */
  1046.502261, 1108.730524, 1174.659072, 1244.507935,
  1318.510228, 1396.912926, 1479.977691, 1567.981744,
  1661.218790, 1760.000000, 1864.655046, 1975.533205,
  /* C8 -  */
  2093.004522, 2217.461048, 2349.318143, 2489.015870,
  2637.020455, 2793.825851, 2959.955382, 3135.963488,
  3322.437581, 3520.000000, 3729.310092, 3951.066410,
  /* C9 -  */
  4186.009045, 4434.922096, 4698.636287, 4978.031740,
  5274.040911, 5587.651703, 5919.910763, 6271.926976,
  6644.875161, 7040.000000, 7458.620184, 7902.132820,
  /* C10 - G10  */
  8372.018090, 8869.844191, 9397.272573, 9956.063479,
  10548.081821, 11175.303406, 11839.821527, 12543.853951
};

/* get std MIDI note from frequency
 */
int
get_note (double freq)
{
  extern double adj_pitch;
  extern double pitch_shift;
  extern int n_pitch;

  const double factor = 1.731234049066756242e+01; /* 12/log(2)  */
  double dnote;
  int inote;
  /* MIDI note # 69 is A5(440Hz)  */
  dnote = 69.5 + factor * log(freq/440.0) + adj_pitch;
  inote = (int)dnote;

  /* calc pitch_shift  */
  pitch_shift += (dnote - (double)inote);
  n_pitch ++;

  return inote;
}

/* write midi header
 */
int
smf_header_fmt (int fd,
		unsigned short format,
		unsigned short tracks,
		unsigned short divisions)
{
  int num;

  num = write (fd, "MThd", 4);
  num += wblong (fd, 6); /* head data size (= 6)  */
  num += wbshort (fd, format);
  num += wbshort (fd, tracks);
  num += wbshort (fd, divisions);
  return num;
  /* num = 14  */
}

/* write program change
 */
int
smf_prog_change (int fd, char channel, char prog)
{
  unsigned char data[3];

  data[0] = 0x00; /* delta time  */
  data[1] = 0xC0 + channel;
  data[2] = prog;
  return write (fd, data, 3);
}

/* write tempo
 *  tempo : microseconds per quarter note
 *          0x07A120 (or 500,000) microseconds (= 0.5 sec) for 120 bpm
 */
int
smf_tempo (int fd, unsigned long tempo)
{
  unsigned char data[7];

  data[0] = 0x00; /* delta time  */
  data[1] = 0xff; /* meta */
  data[2] = 0x51; /* tempo */
  data[3] = 0x03; /* bytes */
  data[4] = (char) (tempo >> 16) & 0xff;
  data[5] = (char) (tempo >>  8) & 0xff;
  data[6] = (char) (tempo      ) & 0xff;
  return write (fd, data, 7);
}

/* note on
 */
int
smf_note_on (int fd, long dtime, char note, char vel, char channel)
{
  unsigned char data[3];
  int num;

  num = write_var_len (fd, dtime);
  data[0] = 0x90 + channel;
  data[1] = note;
  data[2] = vel;
  num += write (fd, data, 3);
  return num;
}

/* note off
 */
int
smf_note_off (int fd, long dtime, char note, char vel, char channel)
{
  unsigned char data[3];
  int num;

  num = write_var_len (fd, dtime);
  data[0] = 0x80 + channel;
  data[1] = note;
  data[2] = vel;
  num += write (fd, data, 3);
  return num;
}

/* write track head
 */
int
smf_track_head (int fd, unsigned long size)
{
  int num;
  num = write (fd, "MTrk", 4);
  num += wblong (fd, size);
  return num;
  /* num = 8  */
}

/* write track end
 */
int
smf_track_end (int fd)
{
  unsigned char data[4];
  data[0] = 0x00; /* delta time  */
  data[1] = 0xff;
  data[2] = 0x2f;
  data[3] = 0x00;
  return write (fd, data, 4);
}

/* write/read variable-length variable
 */
int
write_var_len (int fd, long value)
{
  unsigned char rep[4];
  int bytes;

  bytes = 1;
  rep[3] = value & 0x7f;
  value >>= 7;
  while (value >0)
    {
      rep[3 - bytes] = (value & 0x7f) | 0x80;
      bytes ++;
      value >>= 7;
    }

  return (write (fd, &rep[4-bytes], bytes));
}

int
read_var_len (int fd, long *value)
{
  unsigned char c;
  int bytes;

  *value = 0;
  bytes = 0;
  do
    {
      (*value) <<= 7;
      bytes += read (fd, &c, 1);
      (*value) += (c & 0x7f);
    }
  while ( (c & 0x80) == 0x80 );

  return bytes;
}

/* Write long, big-endian: big end first.
 * RETURN VAULE
 *  written bytes
 */
int
wblong (int fd, unsigned long ul)
{
  unsigned char data[4];
  data[0] = (char) (ul >> 24) & 0xff;
  data[1] = (char) (ul >> 16) & 0xff;
  data[2] = (char) (ul >> 8) & 0xff;
  data[3] = (char) (ul) & 0xff;
  return write (fd, data, 4);
}

/* Write short, big-endian: big end first.
 * RETURN VAULE
 *  written bytes
 */
int
wbshort (int fd, unsigned short us)
{
  unsigned char data[2];
  data[0] = (char) (us >> 8) & 0xff;
  data[1] = (char) (us) & 0xff;
  return write (fd, data, 2);
}
