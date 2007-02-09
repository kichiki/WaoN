/* header file for snd.c --
 * some wrapper for libsndfile
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: snd.h,v 1.1 2007/02/09 05:58:38 kichiki Exp $
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
#ifndef	_SND_H_
#define	_SND_H_


long sndfile_read (SNDFILE *sf, SF_INFO sfinfo,
		   double * left, double * right,
		   int len);

long sndfile_read_at (SNDFILE *sf, SF_INFO sfinfo,
		      long start,
		      double * left, double * right,
		      int len);

/* print sfinfo
 */
void sndfile_print_info (SF_INFO *sfinfo);

/*
 * OUTPUT (returned value)
 *  -1 : no extension or unsupported format
 *  0  : wav
 *  1  : flac
 */
int check_filetype_by_extension (const char *file);

/* output functions
 */
SNDFILE * sndfile_open_for_write (SF_INFO *sfinfo,
				  const char *file,
				  int samplerate,
				  int channels);

long sndfile_write (SNDFILE *sf, SF_INFO sfinfo,
		    double * left, double * right,
		    int len);

#endif /* !_SND_H_ */
