# If you installed rtlsdr/gnuradio with PyBOMBS, you might want to do
# something like "source /path/to/gnuradio/setup_env.sh"

CC?=gcc
CFLAGS?=-O2 -g
LDFLAGS?=-lglut -lGL -lfftw3 -lm
RTLFLAGS=`pkg-config --cflags --libs librtlsdr`
PREFIX?=/usr/local
PROGRAM=rtl_waterfall

all: 
	$(CC) $(CFLAGS) -o $(PROGRAM) $(PROGRAM).c $(LDFLAGS) $(RTLFLAGS)

install:
	mkdir -p $(PREFIX)/bin
	cp $(PROGRAM) $(PREFIX)/bin

clean:
	rm -f $(PROGRAM) *.o
