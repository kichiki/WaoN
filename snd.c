/* some wrapper for libsndfile
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: snd.c,v 1.4 2007/10/21 04:03:30 kichiki Exp $
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
#include <string.h> // memset()
#include <sndfile.h>

#include "memory-check.h" // CHECK_MALLOC() macro


long sndfile_read (SNDFILE *sf, SF_INFO sfinfo,
		   double * left, double * right,
		   int len)
{
  static double *buf = NULL;
  static int nbuf = 0;

  if (buf == NULL)
    {
      buf = (double *)malloc (sizeof (double) * len * sfinfo.channels);
      CHECK_MALLOC (buf, "sndfile_read");
      nbuf = len * sfinfo.channels;
    }
  if (len * sfinfo.channels > nbuf)
    {
      buf = (double *)realloc (buf, sizeof (double) * len * sfinfo.channels);
      CHECK_MALLOC (buf, "sndfile_read");
      nbuf = len * sfinfo.channels;
    }

  sf_count_t status;

  if (sfinfo.channels == 1)
    {
      status = sf_readf_double (sf, left, (sf_count_t)len);
    }
  else
    {
      status = sf_readf_double (sf, buf, (sf_count_t)len);
      int i;
      for (i = 0; i < len; i ++)
	{
	  left  [i] = buf [i * sfinfo.channels];
	  right [i] = buf [i * sfinfo.channels + 1];
	}
    }

  return ((long) status);
}

long sndfile_read_at (SNDFILE *sf, SF_INFO sfinfo,
		      long start,
		      double * left, double * right,
		      int len)
{
  sf_count_t status;

  // check the range
  if (start < 0) return 0;
  else if (start >= sfinfo.frames) return 0;

  // seek the point start
  status = sf_seek  (sf, (sf_count_t)start, SEEK_SET);
  if (status == -1)
    {
      fprintf (stderr, "seek error\n");
      exit (1);
    }

  return (sndfile_read (sf, sfinfo, left, right, len));
}


/* print sfinfo
 */
void sndfile_print_info (SF_INFO *sfinfo)
{
  // Major
  switch (sfinfo->format & SF_FORMAT_TYPEMASK)
    {
    case SF_FORMAT_WAV:
      fprintf (stdout, "Format: Microsoft WAV format (little endian default).\n");
      break;

    case SF_FORMAT_AIFF:
      fprintf (stdout, "Format: Apple/SGI AIFF format (big endian).\n");
      break;

    case SF_FORMAT_AU:
      fprintf (stdout, "Format: Sun/NeXT AU format (big endian).\n");
      break;

    case SF_FORMAT_RAW:
      fprintf (stdout, "Format: RAW PCM data.\n");
      break;

    case SF_FORMAT_PAF:
      fprintf (stdout, "Format: Ensoniq PARIS file format.\n");
      break;

    case SF_FORMAT_SVX:
      fprintf (stdout, "Format: Amiga IFF / SVX8 / SV16 format.\n");
      break;

    case SF_FORMAT_NIST:
      fprintf (stdout, "Format: Sphere NIST format.\n");
      break;

    case SF_FORMAT_VOC:
      fprintf (stdout, "Format: VOC files.\n");
      break;

    case SF_FORMAT_IRCAM:
      fprintf (stdout, "Format: Berkeley/IRCAM/CARL\n");
      break;

    case SF_FORMAT_W64:
      fprintf (stdout, "Format: Sonic Foundry's 64 bit RIFF/WAV\n");
      break;

    case SF_FORMAT_MAT4:
      fprintf (stdout, "Format: Matlab (tm) V4.2 / GNU Octave 2.0\n");
      break;

    case SF_FORMAT_MAT5:
      fprintf (stdout, "Format: Matlab (tm) V5.0 / GNU Octave 2.1\n");
      break;

    case SF_FORMAT_PVF:
      fprintf (stdout, "Format: Portable Voice Format\n");
      break;

    case SF_FORMAT_XI:
      fprintf (stdout, "Format: Fasttracker 2 Extended Instrument\n");
      break;

    case SF_FORMAT_HTK:
      fprintf (stdout, "Format: HMM Tool Kit format\n");
      break;

    case SF_FORMAT_SDS:
      fprintf (stdout, "Format: Midi Sample Dump Standard\n");
      break;

    case SF_FORMAT_AVR:
      fprintf (stdout, "Format: Audio Visual Research\n");
      break;

    case SF_FORMAT_WAVEX:
      fprintf (stdout, "Format: MS WAVE with WAVEFORMATEX\n");
      break;

    case SF_FORMAT_SD2:
      fprintf (stdout, "Format: Sound Designer 2\n");
      break;

    case SF_FORMAT_FLAC:
      fprintf (stdout, "Format: FLAC lossless file format\n");
      break;

    case SF_FORMAT_CAF:
      fprintf (stdout, "Format: Core Audio File format\n");
      break;

    default :
      fprintf (stderr, "unknown Subtype\n");
      exit (1);
    }

  // Subtypes
  switch (sfinfo->format & SF_FORMAT_SUBMASK)
    {
    case SF_FORMAT_PCM_S8:
      fprintf (stdout, "Subtype: Signed 8 bit data\n");
      break;

    case SF_FORMAT_PCM_16:
      fprintf (stdout, "Subtype: Signed 16 bit data\n");
      break;

    case SF_FORMAT_PCM_24:
      fprintf (stdout, "Subtype: Signed 24 bit data\n");
      break;

    case SF_FORMAT_PCM_32:
      fprintf (stdout, "Subtype: Signed 32 bit data\n");
      break;

    case SF_FORMAT_PCM_U8:
      fprintf (stdout, "Subtype: Unsigned 8 bit data (WAV and RAW only)\n");
      break;

    case SF_FORMAT_FLOAT:
      fprintf (stdout, "Subtype: 32 bit float data\n");
      break;

    case SF_FORMAT_DOUBLE:
      fprintf (stdout, "Subtype: 64 bit float data\n");
      break;

    case SF_FORMAT_ULAW:
      fprintf (stdout, "Subtype: U-Law encoded.\n");
      break;

    case SF_FORMAT_ALAW:
      fprintf (stdout, "Subtype: A-Law encoded.\n");
      break;

    case SF_FORMAT_IMA_ADPCM:
      fprintf (stdout, "Subtype: IMA ADPCM.\n");
      break;

    case SF_FORMAT_MS_ADPCM:
      fprintf (stdout, "Subtype: Microsoft ADPCM.\n");
      break;

    case SF_FORMAT_GSM610:
      fprintf (stdout, "Subtype: GSM 6.10 encoding.\n");
      break;

    case SF_FORMAT_VOX_ADPCM:
      fprintf (stdout, "Subtype: Oki Dialogic ADPCM encoding.\n");
      break;

    case SF_FORMAT_G721_32:
      fprintf (stdout, "Subtype: 32kbs G721 ADPCM encoding.\n");
      break;

    case SF_FORMAT_G723_24:
      fprintf (stdout, "Subtype: 24kbs G723 ADPCM encoding.\n");
      break;

    case SF_FORMAT_G723_40:
      fprintf (stdout, "Subtype: 40kbs G723 ADPCM encoding.\n");
      break;

    case SF_FORMAT_DWVW_12:
      fprintf (stdout, "Subtype: 12 bit Delta Width Variable Word encoding.\n");
      break;

    case SF_FORMAT_DWVW_16:
      fprintf (stdout, "Subtype: 16 bit Delta Width Variable Word encoding.\n");
      break;

    case SF_FORMAT_DWVW_24:
      fprintf (stdout, "Subtype: 24 bit Delta Width Variable Word encoding.\n");
      break;

    case SF_FORMAT_DWVW_N:
      fprintf (stdout, "Subtype: N bit Delta Width Variable Word encoding.\n");
      break;

    case SF_FORMAT_DPCM_8:
      fprintf (stdout, "Subtype: 8 bit differential PCM (XI only)\n");
      break;

    case SF_FORMAT_DPCM_16:
      fprintf (stdout, "Subtype: 16 bit differential PCM (XI only)\n");
      break;

    default :
      fprintf (stderr, "unknown Subtype\n");
      exit (1);
    }

  // Endian
  switch (sfinfo->format & SF_FORMAT_ENDMASK)
    {
    case SF_ENDIAN_FILE:
      fprintf (stdout, "Endian type: Default file endian-ness.\n");
      break;

    case SF_ENDIAN_LITTLE:
      fprintf (stdout, "Endian type: Force little endian-ness.\n");
      break;

    case SF_ENDIAN_BIG:
      fprintf (stdout, "Endian type: Force big endian-ness.\n");
      break;

    case SF_ENDIAN_CPU:
      fprintf (stdout, "Endian type: Force CPU endian-ness.\n");
      break;

    default :
      fprintf (stderr, "unknown Endian type\n");
      exit (1);
    }

  fprintf (stdout, "frames     : %d\n", (int)sfinfo->frames);
  fprintf (stdout, "samplerate : %d\n", sfinfo->samplerate);
  fprintf (stdout, "channels   : %d\n", sfinfo->channels);
  fprintf (stdout, "sections   : %d\n", sfinfo->sections);
  fprintf (stdout, "seekable   : %d\n", sfinfo->seekable);
}


/*
 * OUTPUT (returned value)
 *  -1 : no extension or unsupported format
 *  0  : wav
 *  1  : flac
 */
int check_filetype_by_extension (const char *file)
{
  int len;
  len = strlen (file);

  int i;
  for (i = len-1; i >= 0; i--)
    {
      if (file [i] == '.') break;
    }

  if (file [i] != '.') return -1; // no extension
  else if (strcmp (file + i + 1, "wav") == 0) return 0; // wav
  else if (strcmp (file + i + 1, "flac") == 0) return 1; // flac
  else return -1; // unsupported format
}

/* output functions
 */
SNDFILE * sndfile_open_for_write (SF_INFO *sfinfo,
				  const char *file,
				  int samplerate,
				  int channels)
{
  int mode;

  mode = check_filetype_by_extension (file);
  if (mode < 0)
    {
      fprintf (stderr, "unknown or unsupported format %s."
	       " we force to write in wav format.\n", file);
      mode = 0; // wav
    }

  memset (sfinfo, 0, sizeof (*sfinfo));
  // format
  switch (mode)
    {
    case 0: // wav
      sfinfo->format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_FILE;
      break;

    case 1: // flac
      sfinfo->format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16 | SF_ENDIAN_FILE;
      break;

    default:
      fprintf (stderr, "invalid mode %d\n", mode);
      exit (1);
    }

  //sfinfo->frames = 0;
  sfinfo->samplerate = samplerate;
  sfinfo->channels = channels;
  sfinfo->sections = 1;
  sfinfo->seekable = 1;

  // open
  SNDFILE *sf = NULL;
  sf = sf_open (file, SFM_WRITE, sfinfo);
  if (sf == NULL)
    {
      fprintf (stderr, "fail to open %s\n", file);
      exit (1);
    }

  return (sf);
}


long sndfile_write (SNDFILE *sf, SF_INFO sfinfo,
		    double * left, double * right,
		    int len)
{
  sf_count_t status;

  if (sfinfo.channels == 1)
    {
      status = sf_writef_double (sf, left, (sf_count_t)len);
    }
  else
    {
      double *buf = NULL;
      buf = (double *) malloc (sizeof (double) * len * sfinfo.channels);
      CHECK_MALLOC (buf, "sndfile_write");
      int i;
      for (i = 0; i < len; i ++)
	{
	  buf [i * sfinfo.channels]     = left  [i];
	  buf [i * sfinfo.channels + 1] = right [i];
	}
      status = sf_writef_double (sf, buf, (sf_count_t)len);
      free (buf);
    }

  return ((long) status);
}

