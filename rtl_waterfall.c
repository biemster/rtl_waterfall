/*
 * small program that plots a waterfall using rtlsdr
 * usage: rtl_waterfall freq(MHz) gain
 *
 * glut code copied from http://stackoverflow.com/questions/503816/linux-fastest-way-to-draw
 * rtlsdr code copied from rtl_sdr.c that came with the lib
 *
 * to install the necessary libs on linux: apt-get install libfftw3-3 libfftw3-dev freeglut3-dev libglew-dev
 * on OSX: brew install fftw (glut should be present by default)
 *
 * Compile on linux: gcc -o rtl_waterfall rtl_waterfall.c -lglut -lGL -lrtlsdr -lfftw3 -lm
 * OSX: gcc -framework GLUT -framework OpenGL -framework Cocoa rtl_waterfall.c -o rtl_waterfall -lrtlsdr -lfftw3 -lm
 *
 */

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glut.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <fftw3.h>

#include "rtl-sdr.h"

#define GLUT_BUFSIZE 512

float texture[GLUT_BUFSIZE][GLUT_BUFSIZE][3];
float offset;
#define FBUF_LEN 8 // room for {'1', '7', '6', '6', '.', '0', '\0'}
char strFreq[FBUF_LEN];
float pwr_max;
float pwr_diff;

uint8_t *buffer;
fftw_complex *fftw_in;
fftw_complex *fftw_out;
fftw_plan fftw_p;

#define DEFAULT_SAMPLE_RATE	2e6
#define DEFAULT_BUF_LENGTH			(8 * 16384)		// 2^17, [min,max]=[512,(256 * 16384)], update freq = (DEFAULT_SAMPLE_RATE / DEFAULT_BUF_LENGTH) Hz

// Not all tuners can go to either extreme...
#define RTL_MIN_FREQ 22e6
#define RTL_MAX_FREQ 1766e6

static int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;
uint32_t frequency;
uint32_t samp_rate = DEFAULT_SAMPLE_RATE;

void displayTicks();
void glut_renderScene()
{
	glEnable (GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GLUT_BUFSIZE, GLUT_BUFSIZE, GL_RGB, GL_FLOAT, &texture[0][0][0]);

	glBegin(GL_QUADS);
		glTexCoord2f(0.0f+offset, 0.0f); glVertex2f(-1.0, -1.0);
		glTexCoord2f(0.5f+offset, 0.0f); glVertex2f(-1.0,  1.0);
		glTexCoord2f(0.5f+offset, 1.0f); glVertex2f( 1.0,  1.0);
		glTexCoord2f(0.0f+offset, 1.0f); glVertex2f( 1.0, -1.0);
	glEnd();

	displayTicks();

	glFlush();
	glutSwapBuffers();
}

void glut_keyboard( unsigned char key, int x, int y )
{
	uint32_t new_freq;
	int r;
 	switch(key)
 	{
	case 'Q':
		new_freq = frequency - samp_rate;
		if (new_freq < RTL_MIN_FREQ)
			new_freq = RTL_MIN_FREQ;
		if (rtlsdr_set_center_freq(dev, new_freq) < 0)
			fprintf(stderr, "WARNING: Failed to set center freq.\n");
		else {
			fprintf(stderr, "\rTuned to %f MHz.", new_freq/1e6);
			frequency = new_freq;
		}
		snprintf(strFreq, FBUF_LEN, "%6.1f",frequency/1e6);
		break;
	case 'q':
		new_freq = frequency - samp_rate/2;
		if (new_freq < RTL_MIN_FREQ)
			new_freq = RTL_MIN_FREQ;
		if (rtlsdr_set_center_freq(dev, new_freq) < 0)
			fprintf(stderr, "WARNING: Failed to set center freq.\n");
		else {
			fprintf(stderr, "\rTuned to %f MHz.", new_freq/1e6);
			frequency = new_freq;
		}
		snprintf(strFreq, FBUF_LEN, "%6.1f",frequency/1e6);
		break;
	case 'W':
		new_freq = frequency + samp_rate;
		if (new_freq > RTL_MAX_FREQ)
			new_freq = RTL_MAX_FREQ;
		if (rtlsdr_set_center_freq(dev, new_freq) < 0)
			fprintf(stderr, "WARNING: Failed to set center freq.\n");
		else {
			fprintf(stderr, "\rTuned to %f MHz.", new_freq/1e6);
			frequency = new_freq;
		}
		snprintf(strFreq, FBUF_LEN, "%6.1f",frequency/1e6);
		break;
	case 'w':
		new_freq = frequency + samp_rate/4;
		if (new_freq > RTL_MAX_FREQ)
			new_freq = RTL_MAX_FREQ;
		if (rtlsdr_set_center_freq(dev, new_freq) < 0)
			fprintf(stderr, "WARNING: Failed to set center freq.\n");
		else {
			fprintf(stderr, "\rTuned to %f MHz.", new_freq/1e6);
			frequency = new_freq;
		}
		snprintf(strFreq, FBUF_LEN, "%6.1f",frequency/1e6);
 		break;
 	case 'a':
 		pwr_diff *= .5;
 		fprintf(stderr, "\rpwr_diff reset to %.4f", pwr_diff);
 		break;
 	case 'z':
 		pwr_diff *= 2.;
 		fprintf(stderr, "\rpwr_diff reset to %.4f", pwr_diff);
 		break;
 	case 27: // Escape key
 		fprintf(stderr, "\nbye\n");
 		exit(0);
		break;
	}
}

void displayTicks()
{
	char tbuf[FBUF_LEN];
	snprintf(tbuf, FBUF_LEN, "%0.2f", samp_rate/4e6);
	glDisable(GL_TEXTURE_2D);
		glRasterPos2f(.95,-.98);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, '|');

		glRasterPos2f(.712,-1);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, '|');

		glRasterPos2f(.475,-.98);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, '|');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ' ');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, '+');
		for (int i = 0; i < strlen(tbuf); i++)
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, tbuf[i]);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'M');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'H');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'z');

		glRasterPos2f(.237,-1);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, '|');

		glRasterPos2f(-.005,-.98);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, '|');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ' ');
		for (int i = 0; i < strlen(strFreq); i++)
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, strFreq[i]);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ' ');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'M');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'H');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'z');

		glRasterPos2f(-.245,-1);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, '|');

		glRasterPos2f(-.482,-.98);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, '|');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ' ');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, '-');
		for (int i = 0; i < strlen(tbuf); i++)
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, tbuf[i]);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'M');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'H');
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'z');

		glRasterPos2f(-.72,-1);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, '|');

		glRasterPos2f(-.962,-.98);
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, '|');
	glEnable(GL_TEXTURE_2D);
}

int glut_init(int *argc,char **argv)
{
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(GLUT_BUFSIZE, GLUT_BUFSIZE/2);
	glutInit(argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

	glutCreateWindow("Waterfall");

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GLUT_BUFSIZE, GLUT_BUFSIZE, 0, GL_RGB, GL_FLOAT, &texture[0][0][0]);
	
	glutDisplayFunc(glut_renderScene);

	// add key press check
	// http://stackoverflow.com/questions/10250510/glutmainloop-in-mac-osx-lion-freezes-application
	glutKeyboardFunc(glut_keyboard);
}

void readData(int line_idx)
{	
	// this also works without the /2, the fft only needs to be drawn in the texture once then
	// I'm not sure if this is supposed to work like that, maybe ask some GL expert?
	// use /2 for now, maybe test later for performance
	if(line_idx == GLUT_BUFSIZE/2)
	{
		offset = 0.0f;
		line_idx = 0;
	}
	else offset = -(float)(line_idx)/(float)GLUT_BUFSIZE;

	// scale colors every full round of the buffer
/*	if(line_idx==100)
	{
		pwr_diff = pwr_max;
		fprintf(stderr, "pwr_diff reset to %.2f\n", pwr_diff);
		pwr_max = 0.0f;
	}
*/	
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;
	int n_read;
	int r = rtlsdr_read_sync(dev, buffer, out_block_size, &n_read);
	if (r < 0)
	{
		fprintf(stderr, "WARNING: sync read failed.\n");
		return;
	}
	
	if ((uint32_t)n_read < out_block_size)
	{
		fprintf(stderr, "Short read, samples lost, exiting!\n");
		return;
	}
	
	// calculate fft of buffer
	uint32_t N = out_block_size/2;
	int i;
	for(i = 0 ; i < N ; i++)
	{
		fftw_in[i][0] = (buffer[i*2] -127) * 0.008;		// adc is 8 bits, map (0,255) to (-1,1)
		fftw_in[i][1] = (buffer[i*2 +1] -127) * 0.008;
	}
	fftw_execute(fftw_p);
	//fprintf(stderr, "%d\n", n_read);
	//fprintf(stderr, "%3d: %d %d %d %d %d %d %d %d %d %d\n", line_idx, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9] );
	//fprintf(stderr, "%3d: %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", line_idx, fftw_in[0][0], fftw_in[0][1], fftw_in[1][0], fftw_in[1][1], fftw_in[2][0], fftw_in[2][1], fftw_in[3][0], fftw_in[3][1], fftw_in[4][0], fftw_in[4][1] );
	//fprintf(stderr, "%3d: %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", line_idx, fftw_out[0][0], fftw_out[0][1], fftw_out[1][0], fftw_out[1][1], fftw_out[2][0], fftw_out[2][1], fftw_out[3][0], fftw_out[3][1], fftw_out[4][0], fftw_out[4][1] );

		
	// put fft on screen
	// x = index of pixel on screen, n_avg is number of fft bins that have to be averaged in one pixel
	int n_avg = N / GLUT_BUFSIZE;
	float pwr, color_idx, color_blue, color_green, color_red;
	int x, p;
	for(x = 0 ; x < GLUT_BUFSIZE ; x++)
	{
		pwr = 0.0f;
		for(p = 0 ; p < n_avg ; p++) pwr += (fftw_out[(x*n_avg) +p][0] * fftw_out[(x*n_avg) +p][0]) + (fftw_out[(x*n_avg) +p][1] * fftw_out[(x*n_avg) +p][1]);
		pwr /= (n_avg * (N/2));
		
		// scale colors to power in spectrum
		if(pwr > pwr_max) pwr_max = pwr;
		color_idx = pwr/pwr_diff;
		
		//color_idx = (float)x/(float)GLUT_BUFSIZE;
		if(color_idx < 1)
		{
			//color_blue = (sin((color_idx * 3.1415f) + 1.5708) +1)/2;
			//color_green = sin(color_idx * 3.1415f);
			//color_red = (sin((color_idx * 3.1415f) - 1.5708) +1)/2;
			color_blue = exp( -((color_idx - 0.1) / .3) * ((color_idx - 0.1) / .3) );
			color_green = exp( -((color_idx - 0.35) / .5) * ((color_idx - 0.35) / .5) );
			color_red = exp( -((color_idx - .95) / .3) * ((color_idx - .95) / .3) );
		}
		else
		{
			color_blue = 0.0f;
			color_green = 0.0f;
			color_red = 1.0f;
		}
		
		// negative frequencies are in [N/2,N] and positive in [0,N/2]
		int xN;
		if(x < (GLUT_BUFSIZE/2)) xN = x + GLUT_BUFSIZE/2;
		else xN = x - GLUT_BUFSIZE/2;
		texture[xN][GLUT_BUFSIZE-line_idx-1][0] = color_red;
		texture[xN][GLUT_BUFSIZE-line_idx-1][1] = color_green;
		texture[xN][GLUT_BUFSIZE-line_idx-1][2] = color_blue;
		
		// when using line_idx == GLUT_BUFSIZE/2 in the first if statement the fft should be drawn twice
		texture[xN][(GLUT_BUFSIZE/2)-line_idx-1][0] = color_red;
		texture[xN][(GLUT_BUFSIZE/2)-line_idx-1][1] = color_green;
		texture[xN][(GLUT_BUFSIZE/2)-line_idx-1][2] = color_blue;
	}
	//fprintf(stderr, "%.2f\n", pwr_max);

	glutPostRedisplay();
	glutTimerFunc(0,readData,++line_idx);
}

int main(int argc, char **argv)
{
	int c;
	uint32_t dev_index = 0;
	int gain = 0, ppm = 0;
	frequency = 100e6; /* global */

	// setup window
	glut_init(&argc, argv);

	while ((c = getopt(argc, argv, "d:f:g:p:r:")) != -1) {
		switch (c) {
			case 'd':
				dev_index = atoi(optarg);
				break;
			case 'f':
				frequency = atof(optarg); // for scientific notation
				break;
			case 'g':
				gain = (int) (atof(optarg) * 10);
				break;
			case 'p':
				ppm = (int) atof(optarg);
				break;
			case 'r':
				samp_rate = atof(optarg); // for scientific notation
				break;
			default:
				fprintf(stderr, "Usage: %s [X11_GLUT_flags] [-d <dev_index>] [-f <freq>] "
					"[-g <gain_dB>] [-p ppm] [-r samp_rate]\n", argv[0]);
				exit(1);
				/* NOTREACHED */
		}
	}
	argv += optind;
	argc -= optind;
	
	///
	// init radio
	///
	int device_count = rtlsdr_get_device_count();
	if (!device_count)
	{
		fprintf(stderr, "No supported devices found.\n");
		exit(1);
	}

	fprintf(stderr, "Found %d device(s):\n", device_count);

	fprintf(stderr, "Using device %d: %s\n", dev_index, rtlsdr_get_device_name(dev_index));

	if (rtlsdr_open(&dev, dev_index) < 0)
	{
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
		exit(1);
	}

	/* Set the sample rate */
	if (rtlsdr_set_sample_rate(dev, samp_rate) < 0)
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");

	/* Set the frequency */
	if ((frequency < RTL_MIN_FREQ) || (frequency > RTL_MAX_FREQ)) {
		frequency = 100e6;
		fprintf(stderr, "WARNING: Center frequency should be %dMHz-%dMHz; setting to %dMHz\n",
			(int)(RTL_MIN_FREQ/1e6), (int)(RTL_MAX_FREQ/1e6), (int)(frequency/1e6));
	}
	if (rtlsdr_set_center_freq(dev, frequency) < 0)
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
	else
		fprintf(stderr, "Tuned to %f MHz.\n", frequency/1e6);
	snprintf(strFreq, FBUF_LEN, "%6.1f",frequency/1e6);

	/* Set the oscillator frequency ("PPM") correction */
	if (ppm)
		if (rtlsdr_set_freq_correction(dev, ppm) < 0)
			fprintf(stderr, "WARNING: Failed to set frequency correction\n");

	/* Set the gain */
	if (!gain)
	{
		 /* Enable automatic gain */
		if (rtlsdr_set_tuner_gain_mode(dev, 0) < 0)
			fprintf(stderr, "WARNING: Failed to enable automatic gain.\n");
	}
	else
	{
		/* Enable manual gain */
		if (rtlsdr_set_tuner_gain_mode(dev, 1) < 0)
			fprintf(stderr, "WARNING: Failed to enable manual gain.\n");

		/* Set the tuner gain */
		if (rtlsdr_set_tuner_gain(dev, gain) < 0)
		{
			fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
			fprintf(stderr, "Valid values for e4000 are: -10, 15, 40, 65, 90, 115, 140, 165, 190, 215, 240, 290, 340, 420, 430, 450, 470, 490\n");
			fprintf(stderr, "Valid values for r820t are: 9, 14, 27, 37, 77, 87, 125, 144, 157, 166, 197, 207, 229, 254, 280, 297,\n\t328, 338, 364, 372, 386, 402, 421, 434, 439, 445, 480, 496\n");
			fprintf(stderr, "Gain values are in tenths of dB, e.g. 115 means 11.5 dB.\n");
		}
		else
			fprintf(stderr, "Tuner gain set to %f dB.\n", gain/10.0);
	}
	
	/* Reset endpoint before we start reading from it (mandatory) */
	if (rtlsdr_reset_buffer(dev) < 0)
		fprintf(stderr, "WARNING: Failed to reset buffers.\n");

	
	///
	// setup fftw
	///
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;
	buffer = malloc(out_block_size * sizeof(uint8_t));
	fftw_in = fftw_malloc ( sizeof ( fftw_complex ) * out_block_size/2 );
	fftw_out = fftw_malloc ( sizeof ( fftw_complex ) * out_block_size/2 );
	
	// put the plan on FFTW_MEASURE to calculate the optimal fft plan (takes a few seconds).
	// If performance of FFTW_ESTIMATE is good enough use that one
	//fftw_p = fftw_plan_dft_1d ( out_block_size/2, fftw_in, fftw_out, FFTW_FORWARD, FFTW_MEASURE );
	fftw_p = fftw_plan_dft_1d ( out_block_size/2, fftw_in, fftw_out, FFTW_FORWARD, FFTW_ESTIMATE );

	
	/* start reading samples */
	fprintf(stderr, "Update frequency is %.2fHz.\n",((double)DEFAULT_SAMPLE_RATE / (double)DEFAULT_BUF_LENGTH));
	fprintf(stderr, "Press [Q,q,w,W] to change frequency, [a,z] to adjust waterfall color sensitivity, ESC to quit.\n");
	pwr_max = 0.0f;
	pwr_diff = 1.0f;
	glutTimerFunc(0,readData,0);
	glutMainLoop();
	
	return 0;
}
