// os345interrupts.c - pollInterrupts	06/21/2020
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345config.h"
#include "os345signals.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static void keyboard_isr(void);
static void timer_isr(void);

// **********************************************************************
// **********************************************************************
// global semaphores

extern Semaphore* keyboard;				// keyboard semaphore
extern Semaphore* charReady;				// character has been entered
extern Semaphore* inBufferReady;			// input buffer ready semaphore

extern Semaphore* tics1sec;				// 1 second semaphore
extern Semaphore* tics10thsec;			// 1/10 second semaphore
extern Semaphore* tics10sec;			// 10 second semaphore

extern Semaphore* dcChange;

extern char inChar;				// last entered character
extern int charFlag;				// 0 => buffered input
extern int inBufIndx;				// input pointer into input buffer
extern char inBuffer[INBUF_SIZE+1];	// character input buffer

extern time_t oldTime1;					// old 1sec time
extern time_t oldTime2;					// old 10sec time
extern clock_t myClkTime;
extern clock_t myOldClkTime;

extern int pollClock;				// current clock()
extern int lastPollClock;			// last pollClock

extern int superMode;						// system mode

char** c;
int cSize;
int cIndex;
int check;

extern DC* dc;

// **********************************************************************
// **********************************************************************
// simulate asynchronous interrupts by polling events during idle loop
//
void pollInterrupts(void)
{
	// check for task monopoly
	pollClock = clock();
	assert("Timeout" && ((pollClock - lastPollClock) < MAX_CYCLES));
	lastPollClock = pollClock;

	// check for keyboard interrupt
	if ((inChar = GET_CHAR) > 0)
	{
		keyboard_isr();
	}

	// timer interrupt
	timer_isr();

	return;
} // end pollInterrupts


// **********************************************************************
// keyboard interrupt service routine
//
static void keyboard_isr()
{
	// assert system mode
	assert("keyboard_isr Error" && superMode);

	semSignal(charReady);					// SIGNAL(charReady) (No Swap)
	if (charFlag == 0)
	{
		switch (inChar)
		{
			case '\r':
			case '\n':
			{				
				if (c == NULL)
				{
					c = (char**)malloc(50 * sizeof(char*));
					cSize = 0;
					cIndex = 0;
				}
				c[cSize] = (char*)malloc(strlen(inBuffer));
				strcpy(c[cSize++], inBuffer);
				cIndex = cSize - 1;

				inBufIndx = 0;				// EOL, signal line ready
				semSignal(inBufferReady);	// SIGNAL(inBufferReady)
				break;
			}

			case 0x12:						// ^r
			{
				sigSignal(-1, mySIGCONT);
				break;
			}

			case 0x18:						// ^x
			{
				inBufIndx = 0;
				inBuffer[0] = 0;
				sigSignal(0, mySIGINT);		// interrupt task 0
				semSignal(inBufferReady);	// SEM_SIGNAL(inBufferReady)
				break;
			}

			case 0x17:						// ^w
			{
				sigSignal(-1, mySIGTSTP);
				break;
			}

			case 0x08:
			{
				if (strlen(inBuffer) != 0 && inBufIndx == strlen(inBuffer))
				{
					inBufIndx = strlen(inBuffer) - 1;
					inBuffer[inBufIndx] = 0;
					printf("\b \b");
				}
				break;
			}

			case 0x0F:					// ^o (up)
			{
				if (cSize > 0 && cIndex >= 0) 
				{
					while (strlen(inBuffer) != 0)
					{
						inBufIndx = strlen(inBuffer) - 1;
						inBuffer[inBufIndx] = 0;
						printf("\b \b");
					}
					if (check == 1) { cIndex -= 2; }
					printf("%s", c[cIndex]);
					strcpy(inBuffer, c[cIndex]);
					if (cIndex != 0) { cIndex--; }
					check = 0;
					inBufIndx = strlen(inBuffer);
				}
				break;
			}

			case 0x0C:					// ^l (down)
			{
				if (cSize > 0 && cIndex < cSize)
				{
					while (strlen(inBuffer) != 0)
					{
						inBufIndx = strlen(inBuffer) - 1;
						inBuffer[inBufIndx] = 0;
						printf("\b \b");
					}
					if (check == 0) { cIndex += 2; }
					printf("%s", c[cIndex]);
					strcpy(inBuffer, c[cIndex]);
					if (cIndex != cSize - 1) { cIndex++; }
					check = 1;
					inBufIndx = strlen(inBuffer);
				}
				break;
			}

			case 0x09:					// ^i (left)
			{
				if (inBufIndx > 0)
				{
					inBufIndx--;
					printf("\b");
				}
				break;
			}

			case 0x10:					// ^p (right)
			{
				if (inBufIndx < strlen(inBuffer))
				{
					inBufIndx++;
					printf("%c", inBuffer[inBufIndx - 1]);
				}
				break;
			}

			default:
			{
				if (inBufIndx > 255) { break; } // prevents buffer overflow
				inBuffer[inBufIndx++] = inChar;
				if (inBufIndx == strlen(inBuffer)) { inBuffer[inBufIndx] = 0; }
				printf("%c", inChar);		// echo character
			}
		}
	}
	else
	{
		// single character mode
		inBufIndx = 0;
		inBuffer[inBufIndx] = 0;
	}
	return;
} // end keyboard_isr


// **********************************************************************
// timer interrupt service routine
//
static void timer_isr()
{
	time_t currentTime;						// current time

	// assert system mode
	assert("timer_isr Error" && superMode);

	// capture current time
  	time(&currentTime);

  	// one second timer
  	if ((currentTime - oldTime1) >= 1)
  	{
		// signal 1 second
  	   semSignal(tics1sec);
		oldTime1 += 1;
  	}

	// sample fine clock
	myClkTime = clock();
	if ((myClkTime - myOldClkTime) >= ONE_TENTH_SEC)
	{
		myOldClkTime = myOldClkTime + ONE_TENTH_SEC;									// update old
		semSignal(tics10thsec);
		semSignal(dcChange);
		if (dc->list[dc->size - 1].time > 0) { dc->list[dc->size - 1].time--; }			// Decrement time
		if (dc->list[dc->size - 1].time == 0)											// If time is 0, semSignal that semaphore and remove it from dc
		{
			if (dc->size != 0)
			{
				//printf("semSignaled by dc list: %s\n", dc->list[dc->size - 1].sem->name);
				semSignal(dc->list[dc->size - 1].sem);
				outDC();
			}
		}
	}

	// ?? add other timer sampling/signaling code here for project 2
	// ten second timer
	if ((currentTime - oldTime2) >= 10)
	{
		// signal 10 second
		semSignal(tics10sec);
		oldTime2 += 10;
		fflush(stdout);
	}

	return;
} // end timer_isr
