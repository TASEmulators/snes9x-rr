#include <stdio.h>
#include <stdlib.h>
#include "snes9x.h"
#include "gfx.h"
#include "soundux.h"
#include "ppu.h"
#include "movie.h"
#include "display.h"
#include "logger.h"

#if !(defined(__unix) || defined(__linux) || defined(__sun) || defined(__DJGPP))
#define __builtin_expect(exp,c) ((exp)!=(c))
#endif

int dumpstreams = 0;
int maxframes = -1;

static int resetno = 0;
static int framecounter = 0;

int fastforwardpoint = -1;
int fastforwarddistance = 0;

int keypressscreen = 0;

static int drift = 0;

FILE *video=NULL, *audio=NULL;
char autodemo[128] = "";

int Logger_FrameCounter()
{
	return framecounter;
}

void Logger_NextFrame()
{
	framecounter++;
}

void breakpoint()
{

}

void ResetLogger()
{
	char buffer[256*224*4];

	if (!dumpstreams)
		return;

	framecounter = 0;
	drift=0;

	if (!resetno) // don't create multiple dumpfiles because of resets
	{
	if (video)
		fclose(video);
	if (audio)
		fclose(audio);

	sprintf(buffer, "videostream%d.dat", resetno);
	video = fopen(buffer, "wb");
	if (!video)
	{
		printf("Opening %s failed. Logging cancelled.\n", buffer);
		return;
	}
	
	sprintf(buffer, "audiostream%d.dat", resetno);
	audio = fopen(buffer, "wb");
	if (!audio)
	{
		printf("Opening %s failed. Logging cancelled.\n", buffer);
		fclose(video);
		return;
	}

	char *logo = getenv("LOGO");
	if (!logo)
		logo = "logo.dat";
	FILE *l = fopen(logo, "rb");
	if (l)
	{
		const int soundsize = (so.sixteen_bit ? 2 : 1)*(so.stereo?2:1)*so.playback_rate * Settings.FrameTime / 1000000;
		printf("Soundsize: %d\n", soundsize);
		while (!feof(l))
		{
			if (fread(buffer, 1024,224, l) != 224)
				break;
			VideoLogger(buffer, 256, 224, 24);
			memset(buffer, 0, soundsize);
			AudioLogger(buffer, soundsize);
		}
		fclose(l);
	}
	}
	resetno++;
}

char message[128];
int messageframe;


void VideoLogger(void *pixels, int width, int height, int depth)
{
	int fc = S9xMovieGetFrameCounter();
	if (fc > 0)
		framecounter = fc;
	else
		framecounter++;	

	if (video)
	{
//		if (width != 256 || height != 224)
//		{
//			fprintf(stderr, "\nVideoLogger: Warning! width=%d, height=%d\n", width, height);
//			breakpoint();
//		}
		// This stuff is only good for unix code, but since nitsuja broke unix, I might as well break windows
		fwrite(pixels, 1024, 224, video);
		fflush(video);
		fflush(audio);
		drift++;

		if (maxframes > 0 && __builtin_expect(framecounter >= maxframes, 0))
		{
			printf("-maxframes hit\ndrift:%d\n",drift);
			S9xExit();
		}	
		
	}

	if (Settings.DisplayPressedKeys || keypressscreen)
	{
		static char buffer[128];
                sprintf(buffer, "%s  %s  %s  %s  %s  %s  %c%c%c%c%c%c",
		IPPU.Joypads[0] & SNES_START_MASK ? "Start" : "_____",
		IPPU.Joypads[0] & SNES_SELECT_MASK ? "Select" : "______",
                IPPU.Joypads[0] & SNES_UP_MASK ? "Up" : "__",
		IPPU.Joypads[0] & SNES_DOWN_MASK ? "Down" : "____",
		IPPU.Joypads[0] & SNES_LEFT_MASK ? "Left" : "____",
		IPPU.Joypads[0] & SNES_RIGHT_MASK ? "Right" : "_____",
		IPPU.Joypads[0] & SNES_A_MASK ? 'A':'_',
		IPPU.Joypads[0] & SNES_B_MASK ? 'B':'_',
                IPPU.Joypads[0] & SNES_X_MASK ? 'X':'_',
                IPPU.Joypads[0] & SNES_Y_MASK ? 'Y':'_',  
		IPPU.Joypads[0] & SNES_TL_MASK ? 'L':'_',
		IPPU.Joypads[0] & SNES_TR_MASK ? 'R':'_'
		/*framecounter*/);
		if (Settings.DisplayPressedKeys)
			fprintf(stderr, "%s %d           \r", buffer, framecounter);
		if (keypressscreen)
                    S9xSetInfoString(buffer);
	}
	
	if (__builtin_expect(messageframe >= 0 && framecounter == messageframe, 0))
	{
		S9xMessage(S9X_INFO, S9X_MOVIE_INFO, message);
		GFX.InfoStringTimeout = 300;
		messageframe = -1;
	}

	if (__builtin_expect(fastforwardpoint >= 0 && framecounter >= fastforwardpoint, 0))
	{
//		Settings.FramesToSkip = fastforwarddistance;
		fastforwardpoint = -1;
	}
}


void AudioLogger(void *samples, int length)
{
	if (audio)
		fwrite(samples, 1, length, audio);
	drift--;
}
