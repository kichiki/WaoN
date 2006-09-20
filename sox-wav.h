/*
 * Copyright (C) 1998 Kengo ICHIKI (ichiki@geocities.com)
 * $Id: sox-wav.h,v 1.1 2006/09/20 21:26:46 kichiki Exp $
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

#include <stdio.h> /* FILE  */

#define LEFT(datum, bits)	((datum) << bits) 
#define RIGHT(datum, bits)	((datum) >> bits)

struct soundstream {
	long	rate;		/* sampling rate */
	int	size;		/* word length of data */
	int	style;		/* format of sample numbers */
	int	channels;	/* number of sound channels */
	long	num_samp;	/* number of samples */
	char	swap;		/* do byte- or word-swap */
	FILE	*fp;		/* File stream pointer */
};
typedef struct soundstream *ft_t;

/* Size field */
#define	BYTE	1
#define	WORD	2
#define	DWORD	4
#define	FLOAT	5
#define DOUBLE	6
#define IEEE	7		/* IEEE 80-bit floats.  Is it necessary? */

/* Style field */
#define UNSIGNED	1	/* unsigned linear: Sound Blaster */
#define SIGN2		2	/* signed linear 2's comp: Mac */
#define	ULAW		3	/* U-law signed logs: US telephony, SPARC */
#define ALAW		4	/* A-law signed logs: non-US telephony */
#define ADPCM		5	/* Compressed PCM */
#define GSM		6	/* GSM 6.10 33-byte frame lossy compression */


/* wav.h - various structures and defines used by WAV converter.
 * purloined from public Microsoft RIFF docs
 */
#define	WAVE_FORMAT_UNKNOWN		(0x0000)
#define	WAVE_FORMAT_PCM			(0x0001) 
#define	WAVE_FORMAT_ADPCM		(0x0002)
#define	WAVE_FORMAT_ALAW		(0x0006)
#define	WAVE_FORMAT_MULAW		(0x0007)
#define	WAVE_FORMAT_OKI_ADPCM		(0x0010)
#define WAVE_FORMAT_IMA_ADPCM		(0x0011)
#define	WAVE_FORMAT_DIGISTD		(0x0015)
#define	WAVE_FORMAT_DIGIFIX		(0x0016)
#define	IBM_FORMAT_MULAW         	(0x0101)
#define	IBM_FORMAT_ALAW			(0x0102)
#define	IBM_FORMAT_ADPCM         	(0x0103)

/* function prototypes */
void wavstartread(ft_t ft);
unsigned short  rlshort (FILE *fp);
unsigned long  rllong(FILE *fp);
long wavread (ft_t ft, long *buf, long len);
long rawread (ft_t ft, long *buf, long nsamp);
unsigned short  swapw(unsigned short us);
float swapf(float uf);
unsigned short rshort (ft_t ft);
float rfloat (ft_t ft);
int st_Alaw_to_linear( unsigned char Alawbyte );
int st_ulaw_to_linear ( unsigned char ulawbyte );
char *wav_format_str (unsigned short format_tag);
