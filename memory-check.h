/* macro for checking malloc()
 * Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
 * $Id: memory-check.h,v 1.1 2007/03/10 20:34:38 kichiki Exp $
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
#ifndef	_MEMORY_CHECK_H_
#define	_MEMORY_CHECK_H_


#define CHECK_MALLOC(PTR, FUNC) \
if (PTR == NULL) { \
  fprintf (stderr, FUNC " : malloc error for " #PTR "\n"); \
  exit (1); }


#endif /* !_MEMORY_CHECK_H_ */
