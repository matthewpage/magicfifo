/*
 * magicfifo.c
 *
 *  Created on: 	14 Feb 2010
 *  Last modified:	19 June 2010
 *  Author: 		Matthew Page
 */
#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<stdbool.h>
#include 	<stdarg.h>
#include 	<dlfcn.h>
#include	<syslog.h>

#define VERSION 	1.2
#define BUILD_DATE	"19 June 2010"

int debugLevel = 2;

char *inputPath;
char *outputPath;
int fileStartOffset;			// how many bytes do we move from start
int waitTime;					// how long to wait before next read

bool continueCompression;
bool reachedEndOfFile;

void usage(void)
{
	fprintf(stdout, " Usage : magicfifo -i -o [-s] [-w] [-v <0-7>]\n\n");
	fprintf(stderr, "\t-i : input file\n");
	fprintf(stderr, "\t-o : output file\n");
	fprintf(stderr, "\t-s : byte offset from start of input file\n");
	fprintf(stderr, "\t-w : seconds to wait before reading after EOF\n");
	fprintf(stderr, "\t-v <0-7> : debug level (default : no debug, see sys/syslog.h for levels)\n");
	fprintf(stderr, "\n");
}

static void debug(int level, const char * template, ...)
{
	va_list ap;
	
	va_start (ap, template);
	if (debugLevel < level) return;
	
	vfprintf(stderr, template, ap);
	fprintf(stderr,"\n");
	fflush(stderr);
	
	va_end (ap);
	return;
}

static void readArguments(int argc, char** argv)
{
	int opts;
	for (opts = 1; opts < argc; opts++) {

		if (argv[opts][0] == '-') {
			switch (argv[opts][1]) {
			case 'i':
				inputPath = argv[++opts];				
				break;
			case 'o':
				outputPath = argv[++opts];
				break;
			case 's':
				fileStartOffset = atoi(argv[++opts]);
				break;
			case 'w':
				waitTime = atoi(argv[++opts]);
				break;
			case 'v':
				debugLevel = atoi(argv[++opts]);
				if (debugLevel < 0) debugLevel = 0;
				if (debugLevel > 7) debugLevel = 7;
			}
		}
	}

	return;
}

int main(int argc, char** argv)
{
	fprintf(stdout, "MagicFIFO v%g (c)Matthew Page, %s\n", VERSION, BUILD_DATE);
	
	/* Initialise variables */
	
	long filePosition; 				// position in input file when we have to reopen	
	unsigned char buffer[65536];	// buffer for moving data
	fileStartOffset = 0;
	waitTime = 3;
	
	FILE *fpi;	// pointer to input file
	FILE *fpo;	// pointer to output file
	
	// parse command line arguments
	readArguments(argc, argv);
	
	if (inputPath == NULL || outputPath == NULL)
	{
		debug(LOG_ALERT, "ERROR: You must provide input and output files to MagicFIFO");
		usage();
		return 1;
	}
	else
	{
		debug(LOG_INFO, "input file: %s", inputPath);
		debug(LOG_INFO, "output file: %s", outputPath);
		debug(LOG_INFO, "file offset: %d", fileStartOffset);
		debug(LOG_INFO, "wait time: %d", waitTime);
	}
	
	fpi = fopen(inputPath, "rb");	
	if (!fpi)
	{
		debug(LOG_ALERT, "ERROR: Can't open input file");
		return 1;
	}

	fpo = fopen(outputPath, "wb+");
	if (!fpo)
	{
		debug(LOG_ALERT, "ERROR: Can't open output file");
		return 1;
	}
	
	fseek(fpi, fileStartOffset, SEEK_SET);

	int read = 1;
	int owrite = 0;
	long totalWritten = 0;

	// START READING FILE
	continueCompression = true;
	reachedEndOfFile = false;
	
	debug(LOG_DEBUG, "Started");
	debug(LOG_DEBUG, "size of buffer = %d", sizeof(buffer));
	
	while (continueCompression == true)
	{
		read = fread(buffer, 1, sizeof(buffer), fpi);	
		
		if (read>0)
		{		
			owrite = (int)fwrite(buffer, 1, read, fpo);		
			
			if (owrite != read)
			{
				debug(LOG_ALERT, "ERROR: There was a problem writing all output data. Read: %d, Write: %d", read, owrite);
				return 1;
			}
							
			totalWritten += (long)owrite;						
			debug(LOG_DEBUG, "Written: %li", totalWritten);
			
			/* we must reset this else it only checks for growing file once */
			if (reachedEndOfFile == true)
			{
				debug(LOG_DEBUG, "File is still growing...");
			}
			reachedEndOfFile = false;	
		}			
		else
		{
			/* no data received from file */		
			if (reachedEndOfFile == true)
			{	
				/* we've already been in this state so the file isn't growing, so give up and finish */				
				continueCompression = false;
				debug(LOG_DEBUG, "File has stopped growing");
			}
			else
			{	
				/* lets wait for a bit and see any more data appears */						
				debug(LOG_DEBUG, "Waiting to see if file is still growing");

				reachedEndOfFile = true;				
				filePosition = ftell(fpi);
				sleep(waitTime);
				
				fclose(fpi);
				fpi = fopen(inputPath, "rb");				
				fseek(fpi, filePosition, SEEK_SET);			
			}
		}	
	}	
	
	fclose(fpi);
	fclose(fpo);
	
	debug(LOG_DEBUG, "Done\n");

	return 0;
}
