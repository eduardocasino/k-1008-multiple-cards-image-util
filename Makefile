# Binary to MOS Papertape format conversion routines.
#
# This utility converts GIMP images in C source code header format to a format
# suitable for display on a KIM-1 with one to four K-1008 cards as described in
# the "Use of the K-1008 for grey scale display, app note #2" document.
#
# (C) 2024 Eduardo Casino under the terms of the General Public License, Version 2
#
# https:#github.com/eduardocasino/k-1008-multiple-cards-image-util
#
TARGET = kimg
SOURCES = kimg.c pap.c ihex.c
HEADERS = pap.h ihex.h

$(TARGET): $(SOURCES)
	$(CC) -o $@ $^ -lm

$(TARGET): $(HEADERS)
