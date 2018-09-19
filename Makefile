# If you installed rtlsdr/gnuradio with PyBOMBS, you might want to do
# something like "source /path/to/gnuradio/setup_env.sh"

CC?=gcc
CFLAGS?=-O2 `pkg-config --cflags librtlsdr` -g
LDFLAGS?=-lglut -lGL -lrtlsdr -lfftw3 -lm `pkg-config --libs librtlsdr`

all: 
	$(CC) $(CFLAGS) -o rtl_waterfall rtl_waterfall.c $(LDFLAGS)

clean:
	rm -f rtl_waterfall
