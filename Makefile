# Makefile for waon package
# Copyright (C) 1998-2007 Kengo Ichiki <kichiki@users.sourceforge.net>
# $Id: Makefile,v 1.5 2007/11/05 02:31:44 kichiki Exp $

RM = rm -f

#CFLAGS += \
#	-Wall -O3 \
#	`pkg-config --cflags fftw3` \
#	`pkg-config --cflags sndfile` \
#	`pkg-config --cflags ao` \
#	`pkg-config --cflags samplerate` \
#	`pkg-config --cflags jack` \
#	`pkg-config --cflags gtk+-2.0`
CFLAGS += \
	-Wall -O3 \
	`pkg-config --cflags fftw3` \
	`pkg-config --cflags sndfile` \
	`pkg-config --cflags ao` \
	`pkg-config --cflags samplerate` \
	`pkg-config --cflags gtk+-2.0`

all:	waon pv gwaon

#------------------------------------------------------------------------------
# waon
waon_LDFLAGS = $(LDFLAGS)

waon_LIBS = $(LIBS) \
	-L/usr/local/lib \
	`pkg-config --libs fftw3` \
	`pkg-config --libs sndfile` \
	-lm

waon_OBJS = \
	main.o \
	notes.o \
	midi.o \
	analyse.o \
	fft.o \
	hc.o \
	snd.o

waon: $(waon_OBJS)
	$(CC) $(waon_LDFLAGS) -o waon $(waon_OBJS) $(waon_LIBS)

#------------------------------------------------------------------------------
# pv
pv_LDFLAGS = $(LDFLAGS)

#pv_LIBS = $(LIBS) \
#	-lncurses \
#	`pkg-config --libs ao` \
#	`pkg-config --libs sndfile` \
#	`pkg-config --libs fftw3` \
#	`pkg-config --libs samplerate` \
#	`pkg-config --libs jack` \
#	-lm

#pv_OBJ = \
#	pv.o \
#	pv-complex.o \
#	pv-conventional.o \
#	pv-ellis.o \
#	pv-freq.o \
#	pv-loose-lock.o \
#	pv-nofft.o\
#	pv-complex-curses.o \
#	hc.o \
#	fft.o \
#	snd.o \
#	ao-wrapper.o \
#	jack-pv.o


pv_LIBS = $(LIBS) \
	-lncurses \
	`pkg-config --libs ao` \
	`pkg-config --libs sndfile` \
	`pkg-config --libs fftw3` \
	`pkg-config --libs samplerate` \
	-lm

pv_OBJ = \
	pv.o \
	pv-complex.o \
	pv-conventional.o \
	pv-ellis.o \
	pv-freq.o \
	pv-loose-lock.o \
	pv-nofft.o\
	pv-complex-curses.o \
	hc.o \
	fft.o \
	snd.o \
	ao-wrapper.o

pv:	$(pv_OBJ)
	$(CC) $(pv_LDFLAGS) -o pv $(pv_OBJ) $(pv_LIBS)

#------------------------------------------------------------------------------
# gwaon
gwaon_LDFLAGS = $(LDFLAGS)

gwaon_LIBS    = $(LIBS)\
	`pkg-config --libs gtk+-2.0` \
	`pkg-config --libs ao` \
	`pkg-config --libs sndfile` \
	`pkg-config --libs fftw3` \
	`pkg-config --libs samplerate` \
	-lm

gwaon_OBJ = \
	gwaon.o \
	gwaon-menu.o \
	gwaon-about.o \
	gwaon-wav.o \
	gwaon-play.o \
	pv-complex.o \
	pv-conventional.o \
	ao-wrapper.o \
	gtk-color.o \
	snd.o \
	hc.o \
	fft.o \
	midi.o

gwaon:	$(gwaon_OBJ)
	$(CC) $(gwaon_LDFLAGS) -o gwaon $(gwaon_OBJ) $(gwaon_LIBS)

#------------------------------------------------------------------------------
clean:
	$(RM) *.o *~ *.core \
	waon \
	pv \
	gwaon
