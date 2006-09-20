/* wav handling routines excerpted from sox-12.15 and modefied
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * Copyright notices on original sources are attached below.
 * $Id: sox-wav.c,v 1.1 2006/09/20 21:26:46 kichiki Exp $
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

#include <stdio.h> /* fread(), feof(), getc()  */
#include <string.h> /* strncmp()  */
#include "sox-wav.h"

char *
sizes[] =
{
  "NONSENSE!",
  "bytes",
  "shorts",
  "NONSENSE",
  "longs",
  "32-bit floats",
  "64-bit floats",
  "IEEE floats"
};
char *
styles[] =
{
  "NONSENSE!",
  "unsigned",
  "signed (2's complement)",
  "u-law",
  "a-law",
  "adpcm",
  "gsm"
};


/* Read wav header
 * from sox-12.15/wav.c
 * deleted ACPCM support for simplicity
 */
void
wavstartread(ft_t ft)
{
  char	magic[4];
  unsigned long	len;

  /* wave file characteristics */
  unsigned long  w_avg_bytes_pre_sec; /* estimate of bytes per second needed */
  unsigned short  w_bits_sample; /* bits per sample */
  unsigned long  bytes_sample; /* bytes per sample (per channel) */
  unsigned short  format_tag;
  unsigned short  block_align;
	
  /* for littlendian check */
  int littlendian;
  char *endptr;

  ft->num_samp = 0;

  /* check little endian */
  ft->swap = 0;
  littlendian = 1;
  endptr = (char *) &littlendian;
  if (!*endptr) ft->swap = 1;
  if ( fread(magic, 1, 4, ft->fp) != 4 || strncmp("RIFF", magic, 4))
    fprintf(stderr, "WaoN Error : RIFF header not found\n");

  len = rllong (ft->fp);

  if ( fread(magic, 1, 4, ft->fp) != 4 || strncmp("WAVE", magic, 4))
    fprintf(stderr, "WaoN Error : WAVE header not found\n");

  /* Now look for the format chunk */
  for (;;)
    {
      if ( fread(magic, 1, 4, ft->fp) != 4 )
	fprintf(stderr, "WaoN Error : WAVE file missing fmt spec\n");
      len = rllong (ft->fp);
      if (strncmp("fmt ", magic, 4) == 0)
	break;				/* Found the format chunk */

      /* skip to next chunk */	
      while (len > 0 && !feof(ft->fp))
	{
	  getc(ft->fp);
	  len--;
	}
    }

  if ( len < 16 )
    fprintf(stderr, "WaoN Error : WAVE file fmt chunk is too short\n");

  format_tag = rlshort (ft->fp);
  len -= 2;
  switch (format_tag)
    {
    case WAVE_FORMAT_UNKNOWN:
      fprintf(stderr, "WaoN Error : WAVE file is in unsupported "
	      "Microsoft Official Unknown format.\n");

    case WAVE_FORMAT_PCM:
      ft->style = -1;
      break;
	
    case WAVE_FORMAT_ALAW:	/* Think I can handle this */
      ft->style = ALAW;
      break;

    case WAVE_FORMAT_MULAW:	/* Think I can handle this */
      ft->style = ULAW;
      break;

    case WAVE_FORMAT_ADPCM:
      fprintf(stderr, "WaoN Error : this WAV file is in ADPCM format.\n");
    case WAVE_FORMAT_IMA_ADPCM:
      fprintf(stderr, "WaoN Error : this WAV file is in IMA ADPCM format.\n");
    case WAVE_FORMAT_OKI_ADPCM:
      fprintf(stderr, "WaoN Error : this WAV file is in OKI ADPCM format.\n");
    case WAVE_FORMAT_DIGISTD:
      fprintf(stderr, "WaoN Error : this WAV file is in Digistd format.\n");
    case WAVE_FORMAT_DIGIFIX:
      fprintf(stderr, "WaoN Error : this WAV file is in Digifix format.\n");
    case IBM_FORMAT_MULAW:
      fprintf(stderr, "WaoN Error : this WAV file is in IBM U-law format.\n");
    case IBM_FORMAT_ALAW:
      fprintf(stderr, "WaoN Error : this WAV file is in IBM A-law format.\n");
    case IBM_FORMAT_ADPCM:
      fprintf(stderr, "WaoN Error : this WAV file is in IBM ADPCM format.\n");
    default:	fprintf(stderr, "WAV file has unknown format type\n");
    }

  ft->channels = rlshort (ft->fp);
  len -= 2;

  ft->rate = rllong (ft->fp);
  len -= 4;

  w_avg_bytes_pre_sec = rllong (ft->fp); /* Average bytes/second */
  block_align = rlshort (ft->fp); /* Block align */
  len -= 6;

  /* bits per sample per channel */	
  w_bits_sample =  rlshort (ft->fp);
  len -= 2;

  bytes_sample = (w_bits_sample + 7)/8;
  switch (bytes_sample)
    {
    case BYTE:
      /* User options take precedence */
      ft->size = BYTE;
      if(ft->style == -1)
	ft->style = UNSIGNED;
      break;
	
    case WORD:
      ft->size = WORD;
      if(ft->style == -1)
	ft->style = SIGN2;
      break;
	
    case DWORD:
      ft->size = DWORD;
      break;
	
    default:
      fprintf(stderr, "WaoN Error : don't understand .wav size\n");
    }

  /* Skip past the rest of any left over fmt chunk */
  while (len > 0 && !feof(ft->fp))
    {
      getc(ft->fp);
      len--;
    }

  /* Now look for the wave data chunk */
  for (;;)
    {
      if ( fread(magic, 1, 4, ft->fp) != 4 )
	fprintf(stderr, "WAVE file has missing data chunk\n");
      len = rllong (ft->fp);
      if (strncmp("data", magic, 4) == 0)
	break; /* Found the data chunk */
      
      while (len > 0 && !feof(ft->fp)) /* skip to next chunk */
	{
	  getc(ft->fp);
	  len--;
	}
    }
    
  ft->num_samp = len / ft->size; /* total samples */
  
  fprintf(stderr, "WaoN : Reading Wave file: %s format,\n",
	  wav_format_str(format_tag));
  fprintf(stderr, "\t%d channel%s, %ld samp/sec, ",
	  ft->channels,
	  ft->channels == 1 ? "" : "s", ft->rate);
  fprintf(stderr, "%lu byte/sec, %u block align,\n",
	  w_avg_bytes_pre_sec, block_align);
  fprintf(stderr, "\t%u bits/samp, %lu data bytes.\n",
	  w_bits_sample, len);
}


/* Read short, little-endian: little end first. VAX/386 style.
 * from sox-12.15/misc.c
 */
unsigned short
rlshort (FILE *fp)
{
  unsigned char uc, uc2;

  uc  = getc(fp);
  uc2 = getc(fp);
  return (uc2 << 8) | uc;
}

/* Read long, little-endian: little end first. VAX/386 style.
 * from sox-12.15/misc.c
 */
unsigned long
rllong(FILE *fp)
{
  unsigned char uc, uc2, uc3, uc4;

  uc  = getc(fp);
  uc2 = getc(fp);
  uc3 = getc(fp);
  uc4 = getc(fp);
  return ((long)uc4 << 24) | ((long)uc3 << 16) | ((long)uc2 << 8) | (long)uc;
}

/* Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 *
 * from sox-12.15/wav.c
 * deleted ACPCM support for simplicity
 */
long
wavread (ft_t ft, long *buf, long len)
{
  long	done;
	
  if (len > ft->num_samp) len = ft->num_samp;

  done = rawread(ft, buf, len);

  if (done == 0 && ft->num_samp != 0)
    fprintf(stderr, "WaoN Warning : Premature EOF on .wav input file\n");

  ft->num_samp -= done;
  return done;
}

/* Read raw file data, and convert it to
 * the sox internal signed long format.
 *
 * from sox-12.15/raw.c
 */
long
rawread (ft_t ft, long *buf, long nsamp)
{
  register long datum;
  long done = 0;

  switch(ft->size)
    {
    case BYTE:
      switch(ft->style)
	{
	case SIGN2:
	  while(done < nsamp)
	    {
	      datum = getc(ft->fp);
	      if (feof(ft->fp))
		return done;
	      /* scale signed up to long's range */
	      *buf++ = LEFT(datum, 24);
	      done++;
	    }
	  return done;
	case UNSIGNED:
	  while(done < nsamp)
	    {
	      datum = getc(ft->fp);
	      if (feof(ft->fp))
		return done;
	      /* Convert to signed */
	      datum ^= 128;
	      /* scale signed up to long's range */
	      *buf++ = LEFT(datum, 24);
	      done++;
	    }
	  return done;
	case ULAW:
	  while(done < nsamp)
	    {
	      datum = getc(ft->fp);
	      if (feof(ft->fp))
		return done;
	      datum = st_ulaw_to_linear(datum);
	      /* scale signed up to long's range */
	      *buf++ = LEFT(datum, 16);
	      done++;
	    }
	  return done;
	case ALAW:
	  while(done < nsamp)
	    {
	      datum = getc(ft->fp);
	      if (feof(ft->fp))
		return done;
	      datum = st_Alaw_to_linear(datum);
	      /* scale signed up to long's range */
	      *buf++ = LEFT(datum, 16);
	      done++;
	    }
	  
	  return done;
	}
      break;
    case WORD:
      switch(ft->style)
	{
	case SIGN2:
	  while(done < nsamp)
	    {
	      datum = rshort(ft);
	      if (feof(ft->fp))
		return done;
	      /* scale signed up to long's range */
	      *buf++ = LEFT(datum, 16);
	      done++;
	    }
	  return done;
	case UNSIGNED:
	  while(done < nsamp)
	    {
	      datum = rshort(ft);
	      if (feof(ft->fp))
		return done;
	      /* Convert to signed */
	      datum ^= 0x8000;
	      /* scale signed up to long's range */
	      *buf++ = LEFT(datum, 16);
	      done++;
	    }
	  return done;
	case ULAW:
	  fprintf(stderr, "WaoN Error :No U-Law support for shorts");
	  return done;
	case ALAW:
	  fprintf(stderr, "WaoN Error : No A-Law support for shorts");
	  return done;
	}
      break;
    case FLOAT:
      while(done < nsamp)
	{
	  datum = rfloat(ft);
	  if (feof(ft->fp))
	    return done;
	  *buf++ = LEFT(datum, 16);
	  done++;
	}
      return done;
    default:
      fprintf(stderr, "WaoN Warning : Drop through in rawread!");
    }
  fprintf(stderr, "WaoN Error : don't have code to read %s, %s\n",
       styles[ft->style], sizes[ft->size]);
  return(0);
}

/* Read short
 * from sox-12.15/misc.c
 */
unsigned short
rshort (ft_t ft)
{
  unsigned short us;

  fread(&us, 2, 1, ft->fp);
  if (ft->swap)
    us = swapw(us);
  return us;
}

/* Read float
 * from sox-12.15/misc.c
 */
float
rfloat (ft_t ft)
{
  float f;

  fread(&f, sizeof(float), 1, ft->fp);
  if (ft->swap)
    f = swapf(f);
  return f;
}

/* Byte swappers
 * from sox-12.15/misc.c
 */
unsigned short
swapw(unsigned short us)
{
  return ((us >> 8) | (us << 8)) & 0xffff;
}

/* return swapped 32-bit float
 * from sox-12.15/misc.c
 */
float
swapf(float uf)
{
  union {
    unsigned long l;
    float f;
  } u;

  u.f= uf;
  u.l= (u.l>>24) | ((u.l>>8)&0xff00) | ((u.l<<8)&0xff0000L) | (u.l<<24);
  return u.f;
}

/* from sox-12.15/libst.c -- st_Alaw_to_linear()
 *
 * A-law routines by Graeme W. Gill.
 * Date: 93/5/7
 *
 * References:
 * 1) CCITT Recommendation G.711
 *
 * These routines were used to create the fast
 * lookup tables.
 */
int
st_Alaw_to_linear( unsigned char Alawbyte )
{
  static int exp_lut[8] = { 0, 264, 528, 1056, 2112, 4224, 8448, 16896 };
  int sign, exponent, mantissa, sample;

  Alawbyte ^= 0x55;
  sign = ( Alawbyte & 0x80 );
  Alawbyte &= 0x7f;			/* get magnitude */
  if (Alawbyte >= 16)
    {
      exponent = (Alawbyte >> 4 ) & 0x07;
      mantissa = Alawbyte & 0x0F;
      sample = exp_lut[exponent] + ( mantissa << ( exponent + 3 ) );
    }
  else
    sample = (Alawbyte << 4) + 8;
  if ( sign == 0 ) sample = -sample;

  return sample;
}

/* from sox-12.15/libst.c -- st_ulaw_to_linear()
 *
 * This routine converts from ulaw to 16 bit linear.
 *
 * Craig Reese: IDA/Supercomputing Research Center
 * 29 September 1989
 *
 * References:
 * 1) CCITT Recommendation G.711  (very difficult to follow)
 * 2) MIL-STD-188-113,"Interoperability and Performance Standards
 *     for Analog-to_Digital Conversion Techniques,"
 *     17 February 1987
 *
 * Input: 8 bit ulaw sample
 * Output: signed 16 bit linear sample
 */
int
st_ulaw_to_linear ( unsigned char ulawbyte )
{
  static int exp_lut[8] = { 0, 132, 396, 924, 1980, 4092, 8316, 16764 };
  int sign, exponent, mantissa, sample;

  ulawbyte = ~ ulawbyte;
  sign = ( ulawbyte & 0x80 );
  exponent = ( ulawbyte >> 4 ) & 0x07;
  mantissa = ulawbyte & 0x0F;
  sample = exp_lut[exponent] + ( mantissa << ( exponent + 3 ) );
  if ( sign != 0 ) sample = -sample;

  return sample;
}

/* Return a string corresponding to the wave format type.
 * from sox-12.15/wav.c
 */
char *
wav_format_str (unsigned short format_tag) 
{
  switch (format_tag)
    {
    case WAVE_FORMAT_UNKNOWN:
      return "Microsoft Official Unknown";
    case WAVE_FORMAT_PCM:
      return "Microsoft PCM";
    case WAVE_FORMAT_ADPCM:
      return "Microsoft ADPCM";
    case WAVE_FORMAT_ALAW:
      return "Microsoft A-law";
    case WAVE_FORMAT_MULAW:
      return "Microsoft U-law";
    case WAVE_FORMAT_OKI_ADPCM:
      return "OKI ADPCM format.";
    case WAVE_FORMAT_IMA_ADPCM:
      return "IMA ADPCM";
    case WAVE_FORMAT_DIGISTD:
      return "Digistd format.";
    case WAVE_FORMAT_DIGIFIX:
      return "Digifix format.";
    case IBM_FORMAT_MULAW:
      return "IBM U-law format.";
    case IBM_FORMAT_ALAW:
      return "IBM A-law";
    case IBM_FORMAT_ADPCM:
      return "IBM ADPCM";
    default:
      return "Unknown";
    }
}

/* Copyright notices in original codes
 *
 * sox-12.15/misc.c & raw.c
 * - rlshort(), rllong(), rshort(), rfloat(), swapw(), swapf(), and rawread()
 *----------------------------------------------------------------------------
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * sox-12.15/wav.c
 * - wavstartread(), wavread(), wav_format_str()
 *----------------------------------------------------------------------------
 * Microsoft's WAVE sound format driver
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * Change History:
 *
 * September 11, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed length bug for IMA and MS ADPCM files.
 *
 * June 1, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed some compiler warnings as reported by Kjetil Torgrim Homme
 *   <kjetilho@ifi.uio.no>.
 *   Fixed bug that caused crashes when reading mono MS ADPCM files. Patch
 *   was sent from Michael Brown (mjb@pootle.demon.co.uk).
 *
 * March 15, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Added support for Microsoft's ADPCM and IMA (or better known as
 *   DVI) ADPCM format for wav files.  Info on these formats
 *   was taken from the xanim project, written by
 *   Mark Podlipec (podlipec@ici.net).  For those pieces of code,
 *   the following copyrights notice applies:
 *
 *    XAnim Copyright (C) 1990-1997 by Mark Podlipec.
 *    All rights reserved.
 * 
 *    This software may be freely copied, modified and redistributed without
 *    fee for non-commerical purposes provided that this copyright notice is
 *    preserved intact on all copies and modified copies.
 * 
 *    There is no warranty or other guarantee of fitness of this software.
 *    It is provided solely "as is". The author(s) disclaim(s) all
 *    responsibility and liability with respect to this software's usage
 *    or its effect upon hardware or computer systems.
 *
 * NOTE: Previous maintainers weren't very good at providing contact
 * information.
 *
 * Copyright 1992 Rick Richardson
 * Copyright 1991 Lance Norskog And Sundry Contributors
 *
 * Fixed by various contributors previous to 1998:
 * 1) Little-endian handling
 * 2) Skip other kinds of file data
 * 3) Handle 16-bit formats correctly
 * 4) Not go into infinite loop
 *
 * User options should override file header - we assumed user knows what
 * they are doing if they specify options.
 * Enhancements and clean up by Graeme W. Gill, 93/5/17
 */
