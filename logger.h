extern int dumpstreams;
extern char autodemo[128];
extern int maxframes;
extern int logger_pivot;

int Logger_FrameCounter();
void Logger_NextFrame();

void ResetLogger();
void VideoLogger(void *pixels, int width, int height, int depth);
void AudioLogger(void *samples, int length);
