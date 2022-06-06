// os345.c - OS Kernel	06/21/2020
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

//#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345signals.h"
#include "os345config.h"
#include "os345lc3.h"
#include "os345fat.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static int scheduler(void);
static int dispatcher(void);

//static void keyboard_isr(void);
//static void timer_isr(void);

int sysKillTask(int taskId);
static int initOS(void);

// **********************************************************************
// **********************************************************************
// global semaphores

Semaphore* semaphoreList;			// linked list of active semaphores

Semaphore* keyboard;				// keyboard semaphore
Semaphore* charReady;				// character has been entered
Semaphore* inBufferReady;			// input buffer ready semaphore

Semaphore* tics1sec;				// 1 second semaphore
Semaphore* tics10thsec;				// 1/10 second semaphore
Semaphore* tics10sec;				// 10 second semaphore

Semaphore* dcChange;				// Checks if deltaclock has changed

// **********************************************************************
// **********************************************************************
// global system variables

TCB tcb[MAX_TASKS];					// task control block
Semaphore* taskSems[MAX_TASKS];		// task semaphore
jmp_buf k_context;					// context of kernel stack
jmp_buf reset_context;				// context of kernel stack
volatile void* temp;				// temp pointer used in dispatcher

int scheduler_mode;					// scheduler mode
int superMode;						// system mode
int curTask;						// current task #
long swapCount;						// number of re-schedule cycles
char inChar;						// last entered character
int charFlag;						// 0 => buffered input
int inBufIndx;						// input pointer into input buffer
char inBuffer[INBUF_SIZE+1];		// character input buffer
//Message messages[NUM_MESSAGES];		// process message buffers

int pollClock;						// current clock()
int lastPollClock;					// last pollClock
bool diskMounted;					// disk has been mounted

time_t oldTime1;					// old 1sec time
time_t oldTime2;
clock_t myClkTime;
clock_t myOldClkTime;

PQ* rq;								// ready priority queue
DC* dc;								// Delta Clock


// **********************************************************************
// **********************************************************************
// OS startup
//
// 1. Init OS
// 2. Define reset longjmp vector
// 3. Define global system semaphores
// 4. Create CLI task
// 5. Enter scheduling/idle loop
//
int main(int argc, char* argv[])
{

	// save context for restart (a system reset would return here...)
	int resetCode = setjmp(reset_context);
	superMode = TRUE;						// supervisor mode

	switch (resetCode)
	{
		case POWER_DOWN_QUIT:				// quit
			powerDown(0);
			printf("\nGoodbye!!");
			return 0;

		case POWER_DOWN_RESTART:			// restart
			powerDown(resetCode);
			printf("\nRestarting system...\n");

		case POWER_UP:						// startup
			break;

		default:
			printf("\nShutting down due to error %d", resetCode);
			powerDown(resetCode);
			return resetCode;
	}

	// output header message
	printf("%s", STARTUP_MSG);

	// initalize OS
	if ( resetCode = initOS()) return resetCode;

	// create global/system semaphores here
	//?? vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

	charReady = createSemaphore("charReady", BINARY, 0);
	inBufferReady = createSemaphore("inBufferReady", BINARY, 0);
	keyboard = createSemaphore("keyboard", BINARY, 1);
	tics1sec = createSemaphore("tics1sec", BINARY, 0);
	tics10thsec = createSemaphore("tics10thsec", BINARY, 0);
	tics10sec = createSemaphore("tics10sec", COUNTING, 0);
	dcChange = createSemaphore("dcChange", BINARY, 0);

	//?? ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^	

	// schedule CLI task
	createTask("myShell",			// task name
					P1_main,		// task
					MED_PRIORITY,	// task priority
					argc,			// task arg count
					argv);			// task argument pointers

	// HERE WE GO................

	// Scheduling loop
	// 1. Check for asynchronous events (character inputs, timers, etc.)
	// 2. Choose a ready task to schedule
	// 3. Dispatch task
	// 4. Loop (forever!)

	while(1)									// scheduling loop
	{
		// check for character / timer interrupts
		pollInterrupts();

		// schedule highest priority ready task
		if ((curTask = scheduler()) < 0) continue;

		// dispatch curTask, quit OS if negative return
		if (dispatcher() < 0) break;
	}											// end of scheduling loop

	// exit os
	longjmp(reset_context, POWER_DOWN_QUIT);
	return 0;
} // end main


int enQ(PQ* pq, TID tid, int priority)
{
	if (pq->size == 0)
	{
		pq->q[0].priority = priority;
		pq->q[0].tid = tid;
	}
	else {
		for (int i = pq->size - 1; i >= 0; --i)
		{
			pq->q[i + 1] = pq->q[i];
			if (pq->q[i].priority < priority)		// Found the lower priority, the index above it should be overwritten with the given parameters
			{
				pq->q[i + 1].tid = tid;
				pq->q[i + 1].priority = priority;
				break;
			}
			if (i == 0)
			{
				pq->q[i].tid = tid;
				pq->q[i].priority = priority;
			}
		}
	}
	pq->size++;
	return tid;
}

int deQ(PQ* pq, TID tid)
{
	if (pq->q == NULL)
	{
		printf("\nWas error. priority queue q was null\n");
		return -1;
	}
	else if (pq->size == 0) { return -2; }
	else if (tid >= 0)							// Delete particular id, for killing a task
	{
		for (int i = 0; i < pq->size; ++i)
		{
			if (pq->q[i].tid == tid)			// Found id, make this tid the return value and delete task from queue
			{
				int taskId = pq->q[i].tid;		// return id
				pq->size -= 1;					// size is 1 fewer
				while (i != pq->size)			// bring all tasks down a value (i stop at size -1 or size? I think this is right. Clear top task?ds)
				{
					pq->q[i] = pq->q[i + 1];
					++i;
				}
				return taskId;					// break and stop for loop
			}
		}
		return -1;
	}
	else if (scheduler_mode == 0)				// Round-Robin mode
	{
		int next = pq->size - 1;
		int taskId = pq->q[next].tid;
		pq->size -= 1;
		return taskId;
	}
	else										// Fair scheduler mode
	{
		for (int i = pq->size - 1; i >= 0; --i)
		{
			if (tcb[pq->q[i].tid].taskTime > 0)	// Found most recent task with time higher than 0
			{
				int taskId = pq->q[i].tid;		// return id
				pq->size -= 1;					// size is 1 fewer
				while (i != pq->size)			// bring all tasks down a value (i stop at size -1 or size? I think this is right. Clear top task?ds)
				{
					pq->q[i] = pq->q[i + 1];
					++i;
				}
				return taskId;						// break and stop for loop
			}
		}
		
		int taskId = pq->q[pq->size - 1].tid;
		pq->size -= 1;
		return taskId;
	}
}

Semaphore* inDC(int time, Semaphore* sem)
{
	if (dc->size == 0)								
	{												SWAP;
		dc->list[0].time = time;					SWAP;
		dc->list[0].sem = sem;						SWAP;
	}
	else {
		for (int i = dc->size - 1; i >= 0; --i)
		{											SWAP;
			dc->list[i + 1] = dc->list[i];			SWAP;
			if (dc->list[i].time > time)					// If time relative to time past is less than i
			{										SWAP;
				dc->list[i + 1].time = time;		SWAP;
				dc->list[i + 1].sem = sem;			SWAP;
				dc->list[i].time -= time;			SWAP;
				break;
			}
			time -= dc->list[i].time;				SWAP;	// Decrement time relative to other semaphores
			if (i == 0)										// Lowest in list
			{										SWAP;
				dc->list[i].time = time;			SWAP;
				dc->list[i].sem = sem;				SWAP;
			}
		}
	}

	dc->size++;										SWAP;
	return sem;										SWAP;
}

Semaphore* outDC()
{
	if (dc->size > 0) 
	{ 
		dc->size--; 
	}
	return dc->list[dc->size -1 ].sem;
}

void printQ(PQ* pq)
{
	//printf("Queue total size: %d\n", pq->size);
	for (int i = 0; i < pq->size; ++i)
	{
		printf("Queue[%d]: \n\ttid: %d\n\tpriority: %d\n\ttime: %d\n", i, pq->q[i].tid, pq->q[i].priority, tcb[pq->q[i].tid].taskTime);
	}
	printf("\n");
	fflush(stdout);
}
int check = 0;
int allocateCycles(int parentTask)
{
	int index = 0;
	int parentTime;
	int childTIDs[MAX_TASKS];

	if (parentTask == 0) { tcb[0].taskTime += 130; }
	parentTime = tcb[parentTask].taskTime;
	
	for (int i = 1; i < 200; ++i)			// Account for all child tids
	{
		if (tcb[i].parent == parentTask && tcb[i].name != NULL)
		{
			childTIDs[index] = i;
			index++;
		}
	}
	
	if (index == 0) { return 0; }									// This task (at tcb[parentTask]) has no children, so return and don't recurse
	for (int i = 0; i < index; ++i)
	{
		tcb[childTIDs[i]].taskTime = parentTime / (index + 1);		// Each child task gets the parent task's current time divided by num siblings
	}
	tcb[parentTask].taskTime = parentTime / (index + 1);			// Parent task gets same, plus the remainder
	tcb[parentTask].taskTime += parentTime % (index + 1);	

	for (int i = 0; i < index; ++i)
	{
		allocateCycles(childTIDs[i]);
	}

	
	return 0;
}


// **********************************************************************
// **********************************************************************
// scheduler
//
static int scheduler()
{  
	int nextTask;										// Tid of the next task

	
	if (scheduler_mode == 0)
	{
		if ((nextTask = deQ(rq, -1)) >= 0)
		{
			enQ(rq, nextTask, tcb[nextTask].priority);
		}
	}
	else												// Fair scheduler
	{
		if ((nextTask = deQ(rq, -1)) >= 0)				// If ready queue is not empty
		{
			if (tcb[nextTask].taskTime == 0) { allocateCycles(0); }
			if (nextTask == -2) { printf("gotcha"); }
			enQ(rq, nextTask, tcb[nextTask].priority);
			tcb[nextTask].taskTime--;
			fflush(stdout);

		}
	}

	// mask sure nextTask is valid
	while (!tcb[nextTask].name)
	{
		if (++nextTask >= MAX_TASKS) nextTask = 0;
	}
	if (tcb[nextTask].signal & mySIGSTOP) return -1;	//<-- Original end

	return nextTask;
} // end scheduler



// **********************************************************************
// **********************************************************************
// dispatch curTask
//
static int dispatcher()
{
	int result;

	// schedule task
	switch(tcb[curTask].state)
	{
		case S_NEW:
		{
			// new task
			printf("\nNew Task[%d] %s", curTask, tcb[curTask].name);
			tcb[curTask].state = S_RUNNING;	// set task to run state

			// save kernel context for task SWAP's
			if (setjmp(k_context))
			{
				superMode = TRUE;					// supervisor mode
				break;								// context switch to next task
			}

			// move to new task stack (leave room for return value/address)
			temp = (int*)tcb[curTask].stack + (STACK_SIZE-8);
			SET_STACK(temp);
			superMode = FALSE;						// user mode

			// begin execution of new task, pass argc, argv
			result = (*tcb[curTask].task)(tcb[curTask].argc, tcb[curTask].argv);

			// task has completed
			if (result) printf("\nTask[%d] returned error %d", curTask, result);
			else printf("\nTask[%d] returned %d", curTask, result);
			tcb[curTask].state = S_EXIT;			// set task to exit state

			// return to kernal mode
			longjmp(k_context, 1);					// return to kernel
		}

		case S_READY:
		{
			tcb[curTask].state = S_RUNNING;			// set task to run
		}

		case S_RUNNING:
		{
			if (setjmp(k_context))
			{
				// SWAP executed in task
				superMode = TRUE;					// supervisor mode
				break;								// return from task
			}
			if (signals()) break;
			longjmp(tcb[curTask].context, 3); 		// restore task context
		}

		case S_BLOCKED:
		{
			break;
		}

		case S_EXIT:
		{
			if (curTask == 0) return -1;			// if CLI, then quit scheduler
			// release resources and kill task
			sysKillTask(curTask);					// kill current task
			break;
		}

		default:
		{
			printf("Unknown Task[%d] State", curTask);
			longjmp(reset_context, POWER_DOWN_ERROR);
		}
	}
	return 0;
} // end dispatcher



// **********************************************************************
// **********************************************************************
// Do a context switch to next task.

// 1. If scheduling task, return (setjmp returns non-zero value)
// 2. Else, save current task context (setjmp returns zero value)
// 3. Set current task state to READY
// 4. Enter kernel mode (longjmp to k_context)

void swapTask()
{
	assert("SWAP Error" && !superMode);		// assert user mode

	// increment swap cycle counter
	swapCount++;

	// either save current task context or schedule task (return)
	if (setjmp(tcb[curTask].context))
	{
		superMode = FALSE;					// user mode
		return;
	}

	// context switch - move task state to ready
	if (tcb[curTask].state == S_RUNNING) tcb[curTask].state = S_READY;

	// move to kernel mode (reschedule)
	longjmp(k_context, 2);
} // end swapTask



// **********************************************************************
// **********************************************************************
// system utility functions
// **********************************************************************
// **********************************************************************

// **********************************************************************
// **********************************************************************
// initialize operating system
static int initOS()
{
	int i;

	// make any system adjustments (for unblocking keyboard inputs)
	INIT_OS

	// reset system variables
	curTask = 0;						// current task #
	swapCount = 0;						// number of scheduler cycles
	scheduler_mode = 0;					// default scheduler
	inChar = 0;							// last entered character
	charFlag = 0;						// 0 => buffered input
	inBufIndx = 0;						// input pointer into input buffer
	semaphoreList = 0;					// linked list of active semaphores
	diskMounted = 0;					// disk has been mounted

	// malloc ready queue
	rq = (PQ*)malloc(sizeof(PQ));
	if (rq == NULL) return 99;
	rq->size = 0;

	// malloc delta clock
	dc = (DC*)malloc(sizeof(DC));
	if (dc == NULL) return 98;
	dc->size = 0;

	// capture current time
	lastPollClock = clock();			// last pollClock
	time(&oldTime1);
	time(&oldTime2);

	// init system tcb's
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;				// tcb
		taskSems[i] = NULL;				// task semaphore
	}

	// init tcb
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;
	}

	// initialize lc-3 memory
	initLC3Memory(LC3_MEM_FRAME, 0xF800>>6);

	tcb[0].taskTime = 0;
	// ?? initialize all execution queues

	return 0;
} // end initOS



// **********************************************************************
// **********************************************************************
// Causes the system to shut down. Use this for critical errors
void powerDown(int code)
{
	int i;
	printf("\nPowerDown Code %d", code);

	// release all system resources.
	printf("\nRecovering Task Resources...");

	// kill all tasks
	for (i = MAX_TASKS-1; i >= 0; i--)
		if(tcb[i].name) sysKillTask(i);

	// delete all semaphores
	while (semaphoreList)
		deleteSemaphore(&semaphoreList);

	// free ready queue
	free(rq);
	

	// ?? release any other system resources
	// ?? deltaclock (project 3)
	free(dc);

	RESTORE_OS
	return;
} // end powerDown

