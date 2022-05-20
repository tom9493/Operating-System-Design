// os345p3.c - Jurassic Park 07/27/2020
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
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
#include "os345park.h"

int carTask(int, char**);
int driverTask(int, char**);
int visitorTask(int, char**);

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;						// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];		// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over
extern DC* dc;
extern TCB tcb[MAX_TASKS];
extern Semaphore* dcChange;

int carID;
int driverID;
int visitorID;
int currentDriverID;
int currentVisID;
char letters[4] = { 'A', 'B', 'C', 'D' };

Semaphore* rideDone[NUM_VISITORS];
Semaphore* driverDone[NUM_DRIVERS];

/* Resource management semaphores */
Semaphore* parkCap;
Semaphore* tickets;
Semaphore* mCap;
Semaphore* gsCap;

/* Mutex semaphores */
Semaphore* requestTicketMutex;
Semaphore* storeVisIDMutex;
Semaphore* storeDvrIDMutex;
Semaphore* needDriver;
Semaphore* dcMutex;
Semaphore* carReleaseMutex;

/* Semaphores (for coordination) */
Semaphore* needTicket;
Semaphore* takeTicket;
Semaphore* wakeupDriver;
Semaphore* driverReady;
Semaphore* carReady;
Semaphore* needVisitor;
Semaphore* visitorReady;
Semaphore* readyForVisitorID;
Semaphore* visitorIDStored;
Semaphore* readyForDriverID;
Semaphore* driverIDStored;
Semaphore* inGiftLine;


/* For testing delta clock */
Semaphore* event[10];
int timeTaskID;

// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);


// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_main(int argc, char* argv[])
{
	char buf[32];
	char* newArgv[2];
	parkMutex = NULL;

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	//?? create car, driver, and visitor tasks here
	//myPark.numInCarLine = myPark.numInPark = 20;			// TEMPORARY -- TESTING CAR TASK
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numOutsidePark = NUM_VISITORS;		SWAP;
	myPark.numInPark = 0;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;

	/* Resource management semaphores */
	parkCap = createSemaphore("park capacity", COUNTING, MAX_IN_PARK);					SWAP;
	tickets = createSemaphore("tickets", COUNTING, MAX_TICKETS);						SWAP;
	mCap = createSemaphore("mueseum capacity", COUNTING, MAX_IN_MUSEUM);				SWAP;
	gsCap = createSemaphore("gift shop capacity", COUNTING, MAX_IN_GIFTSHOP);			SWAP;

	/* Mutex semaphores */
	requestTicketMutex = createSemaphore("request ticket mutex", BINARY, 1);			SWAP;
	storeVisIDMutex = createSemaphore("store visitor id mutex", BINARY, 1);				SWAP;
	storeDvrIDMutex = createSemaphore("store driver id mutex", BINARY, 1);				SWAP;
	needDriver = createSemaphore("need driver", BINARY, 1);								SWAP;
	dcMutex = createSemaphore("delta clock mutex semaphore", BINARY, 1);				SWAP;
	carReleaseMutex = createSemaphore("car release mutex", BINARY, 1);					SWAP;

	/* Semaphores (for coordination) */
	needTicket = createSemaphore("need ticket", BINARY, 0);								SWAP;
	takeTicket = createSemaphore("take ticket", BINARY, 0);								SWAP;
	wakeupDriver = createSemaphore("wakeup driver", BINARY, 0);							SWAP;
	driverReady = createSemaphore("driver ready", BINARY, 0);							SWAP;
	carReady = createSemaphore("car ready", BINARY, 0);									SWAP;
	needVisitor = createSemaphore("need visitor", BINARY, 0);							SWAP;
	visitorReady = createSemaphore("visitor ready", BINARY, 0);							SWAP;
	readyForVisitorID = createSemaphore("ready to store visitor ID", BINARY, 0);		SWAP;
	visitorIDStored = createSemaphore("visitor ID stored in car task", BINARY, 0);		SWAP;
	readyForDriverID = createSemaphore("ready to store driver ID", BINARY, 0);			SWAP;
	driverIDStored = createSemaphore("driver ID stored in car task", BINARY, 0);		SWAP;
	inGiftLine = createSemaphore("in gift line", BINARY, 0);							SWAP;

	for (int i = 0; i < NUM_CARS; ++i)
	{
		sprintf(buf, "carTask%d", carID);		SWAP;
		createTask( buf,
			carTask,
			HIGH_PRIORITY,
			0,
			NULL);				SWAP;
		carID++;				SWAP;

		sprintf(buf, "driverTask%d", driverID);	SWAP;
		createTask(buf,
			driverTask,
			HIGH_PRIORITY,
			0,
			NULL);				SWAP;
		driverID++;				SWAP;
	}

	for (int i = 0; i < NUM_VISITORS; ++i)
	{
		sprintf(buf, "visitorTask%d", visitorID); SWAP;
		createTask(buf,
			visitorTask,
			HIGH_PRIORITY,
			0,
			NULL);				SWAP;
		visitorID++;			SWAP;
	}

	return 0;
} // end project3


int carTask(int argc, char* argv[])
{
	int thisID = carID;						SWAP;
	int visitorIDs[3];						SWAP;
	int thisDriverID;						SWAP;
	while (1)
	{
		for (int i = 0; i < NUM_SEATS; ++i)
		{
			SEM_WAIT(fillSeat[thisID])		SWAP;
			SEM_SIGNAL(needVisitor);		SWAP;
			SEM_WAIT(visitorReady);			SWAP;
			SEM_WAIT(readyForVisitorID);	SWAP;
			visitorIDs[i] = currentVisID;	SWAP;
			SEM_SIGNAL(visitorIDStored);	SWAP;
			
			if (i == NUM_SEATS - 1)
			{
				SEM_SIGNAL(wakeupDriver);			SWAP;
				SEM_WAIT(readyForDriverID);			SWAP;
				thisDriverID = currentDriverID;		SWAP;
				SEM_WAIT(parkMutex);				SWAP;
				myPark.drivers[thisDriverID] = thisID+1;	SWAP;
				SEM_SIGNAL(parkMutex);				SWAP;
				SEM_SIGNAL(driverIDStored);			SWAP;
				SEM_WAIT(driverReady);				SWAP;
				SEM_SIGNAL(carReady);				SWAP;
			}

			SEM_SIGNAL(seatFilled[thisID]);			SWAP;
		}		

		SEM_WAIT(rideOver[thisID]);					SWAP;	// Wait until this car's tour is over, then signal visitors and driver
		/*SEM_WAIT(parkMutex);						SWAP;
		myPark.numInCars--;							SWAP;
		myPark.numInGiftLine++;						SWAP;
		SEM_SIGNAL(parkMutex);						SWAP;*/
		//SEM_WAIT(carReleaseMutex);
		SEM_SIGNAL(driverDone[thisDriverID]);		SWAP;

		for (int i = 0; i < 3; ++i)
		{
			SEM_SIGNAL(rideDone[visitorIDs[i]]);	SWAP;
			SEM_WAIT(inGiftLine);					SWAP;
		}
		//SEM_SIGNAL(carReleaseMutex);
	}
	return 0;
}

int driverTask(int argc, char* argv[])
{
	char buf[32];									SWAP;
	int thisID = driverID;							SWAP;
	sprintf(buf, "driver done %d", thisID);			SWAP;
	driverDone[thisID] = createSemaphore(buf, BINARY, 0);		SWAP;

	while (1) {
		SEM_WAIT(parkMutex);						SWAP;
		myPark.drivers[thisID] = 0;					SWAP;
		SEM_SIGNAL(parkMutex);						SWAP;
		SEM_WAIT(wakeupDriver);						SWAP;
		if (semTryLock(needTicket))
		{											SWAP;
			SEM_WAIT(parkMutex);					SWAP;
			myPark.drivers[thisID] = -1;			SWAP;
			SEM_SIGNAL(parkMutex);					SWAP;
			SEM_WAIT(tickets);						SWAP;
			SEM_SIGNAL(takeTicket)					SWAP;
		}
		else 
		{
			SEM_WAIT(storeDvrIDMutex);				SWAP;	// Only one driver stores id at a time in car task
			currentDriverID = thisID;				SWAP;
			SEM_SIGNAL(readyForDriverID);			SWAP;
			SEM_WAIT(driverIDStored);				SWAP;
			SEM_SIGNAL(storeDvrIDMutex);			SWAP;
			SEM_SIGNAL(driverReady);				SWAP;	// Indicates driver is ready for car tour
			SEM_WAIT(carReady);						SWAP;
			SEM_WAIT(driverDone[thisID]);			SWAP;
		}
	}
}

int visitorTask(int argc, char* argv[])
{
	char buf[32];											SWAP;
	int id = visitorID;										SWAP;	// Unique visitor ID
	sprintf(buf, "line wait %d", id);						SWAP;
	Semaphore* timeSem = createSemaphore(buf, BINARY, 0);	SWAP;
	sprintf(buf, "ride done %d", id);						SWAP;
	rideDone[id] = createSemaphore(buf, BINARY, 0);			SWAP;

	// Arrival
	SEM_WAIT(dcMutex);						SWAP;	// Only one tasks update dcMutex at a time
	inDC(rand() % 100 + 1, timeSem);		SWAP;	// Inserts timing semaphore in dc, random time within 10 seconds (arrival delay)
	SEM_SIGNAL(dcMutex);					SWAP;
	SEM_WAIT(timeSem);						SWAP;	// Enters park after rand time expires and sem signal in delta clock
	
	// Waits outside, gets into park and into ticket line
	SEM_WAIT(parkCap);						SWAP;	// Consumes visitor capacity and blocks if there are too many people there
	SEM_WAIT(parkMutex);					SWAP;	// Only one tasks updates variables at a time (shared memory)
	myPark.numOutsidePark--;				SWAP;	// Update variables
	myPark.numInPark++;						SWAP;
	myPark.numInTicketLine++;				SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;	// Another visitor can enter park and update variables now

	// Waits in ticket line
	SEM_WAIT(dcMutex);						SWAP;
	inDC(rand() % 30 + 1, timeSem);			SWAP;	// Wait in ticket line
	SEM_SIGNAL(dcMutex);					SWAP;
	SEM_WAIT(timeSem);						SWAP;

	// Gets ticekt from driver
	//SEM_WAIT(tickets);						SWAP;	// Requests ticket if more are available
	SEM_WAIT(requestTicketMutex);			SWAP;	// Only one visitor can get a ticket at a time	
	SEM_SIGNAL(needTicket);					SWAP;	// Signals driver that we need a ticket when he wakes up
	SEM_SIGNAL(wakeupDriver);				SWAP;	// Wakes driver up
	SEM_WAIT(takeTicket)					SWAP;	// Can consume a ticket on driver's signal
	SEM_SIGNAL(requestTicketMutex);			SWAP;	// Someone else can get a ticket now

	// Gets in museum line
	SEM_WAIT(parkMutex);					SWAP;
	myPark.numTicketsAvailable--;			SWAP;	// Update variables on UI
	myPark.numInTicketLine--;				SWAP;
	myPark.numInMuseumLine++;				SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;

	// Waits in museum line
	SEM_WAIT(dcMutex);						SWAP;
	inDC(rand() % 30 + 1, timeSem);			SWAP;	// Wait in museum line 
	SEM_SIGNAL(dcMutex);					SWAP;
	SEM_WAIT(timeSem);						SWAP;

	// Gets into museum
	SEM_WAIT(mCap);							SWAP;	// Consumes museum capacity unit
	SEM_WAIT(parkMutex);					SWAP;	// Update variables
	myPark.numInMuseumLine--;				SWAP;	
	myPark.numInMuseum++;					SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;

	// Waits in museum
	SEM_WAIT(dcMutex);						SWAP;
	inDC(rand() % 30 + 1, timeSem);			SWAP;	// Wait in museum
	SEM_SIGNAL(dcMutex);					SWAP;
	SEM_WAIT(timeSem);						SWAP;	
	SEM_SIGNAL(mCap);						SWAP;	// Releases museum capacity unit

	// Leaves museum and gets into car line
	SEM_WAIT(parkMutex);					SWAP;
	myPark.numInMuseum--;					SWAP;	
	myPark.numInCarLine++;					SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;

	// Waits in car line
	SEM_WAIT(dcMutex);						SWAP;
	inDC(rand() % 30, timeSem);				SWAP;	// Wait in car line
	SEM_SIGNAL(dcMutex);					SWAP;
	SEM_WAIT(timeSem);						SWAP;

	// Store visitor ID in car task
	SEM_WAIT(needVisitor);					SWAP;	// Waits till car needs visitor
	SEM_SIGNAL(visitorReady);				SWAP;	// Indicates visitor is ready
	SEM_WAIT(storeVisIDMutex);				SWAP;
	currentVisID = id;						SWAP;
	SEM_SIGNAL(readyForVisitorID);			SWAP;
	SEM_WAIT(visitorIDStored);				SWAP;
	SEM_SIGNAL(storeVisIDMutex);			SWAP;

	// Goes on tour
	SEM_WAIT(parkMutex);					SWAP;
	SEM_SIGNAL(tickets);					SWAP;	// Gives ticket back
	myPark.numTicketsAvailable++;			SWAP;
	myPark.numInCarLine--;					SWAP;
	myPark.numInCars++;						SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;
	SEM_WAIT(rideDone[id]);					SWAP;	// Waits until tour is over
	
	// Tour over, get in giftshop line
	SEM_WAIT(parkMutex);					SWAP;
	myPark.numInCars--;						SWAP;
	myPark.numInGiftLine++;					SWAP;
	SEM_SIGNAL(inGiftLine);					SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;
	

	// Waits in giftshop line
	SEM_WAIT(dcMutex);						SWAP;
	inDC(rand() % 30, timeSem);				SWAP;	// Wait in gift shop line
	SEM_SIGNAL(dcMutex);					SWAP;
	SEM_WAIT(timeSem);						SWAP;
	
	// Goes into giftshop
	SEM_WAIT(gsCap);						SWAP;	// Only goes in if the capacity is not reached
	SEM_WAIT(parkMutex);					SWAP;
	myPark.numInGiftLine--;					SWAP;
	myPark.numInGiftShop++;					SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;

	// Waits in giftshop
	SEM_WAIT(dcMutex);						SWAP;
	inDC(rand() % 30 + 1, timeSem);			SWAP;	// Wait in gift shop 
	SEM_SIGNAL(dcMutex);					SWAP;
	SEM_WAIT(timeSem);						SWAP;
	SEM_SIGNAL(gsCap);						SWAP;	// Releases gsCap resource

	// Exits the giftshop and leaves park
	SEM_WAIT(parkMutex);					SWAP;
	myPark.numInGiftShop--;					SWAP;
	SEM_SIGNAL(parkCap);					SWAP;	// Releases resource, another visitor can enter
	myPark.numInPark--;						SWAP;
	myPark.numExitedPark++;					SWAP;
	SEM_SIGNAL(parkMutex);					SWAP;

}



// functions below test delta clock

// ***********************************************************************
// display all pending events in the delta clock list
void printDeltaClock(void)
{
	int i;
	for (i=0; i<dc->size; i++)
	{
		printf("\n%4d%4d  %-20s", i, dc->list[i].time, dc->list[i].sem->name);
	}
	return;
}

// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[])
{
	int i, flg;
	char buf[32];
	// create some test times for event[0-9]
	int ttime[10] = {
		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};

	for (i=0; i<10; i++)
	{
		sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
		inDC(ttime[i], event[i]);
	}
	printDeltaClock();

	while (dc->size > 0)
	{
		SEM_WAIT(dcChange)
		flg = 0;
		for (i=0; i<10; i++)
		{
			//printf("event[%d]->state: %d\n", i, event[i]->state);
			if (event[i]->state ==1)			{
					printf("\n  event[%d] signaled", i);
					event[i]->state = 0;
					flg = 1;
				}
		}
		if (flg) printDeltaClock();
	}
	printf("\nNo more events in Delta Clock\n");

	// kill dcMonitorTask
	printf("Timer task name: %s\n", tcb[timeTaskID].name);
	killTask(timeTaskID);
	return 0;
} // end dcMonitorTask


extern Semaphore* tics1sec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time
	while (1)
	{
		SWAP;
		SEM_WAIT(tics1sec)
		printf("\nTime = %s", myTime(svtime));
	}
	return 0;
} // end timeTask

int P3_tdc(int argc, char* argv[])
{
	createTask("DC Test",			// task name
		dcMonitorTask,		// task
		10,					// task priority
		argc,					// task arguments
		argv);

	timeTaskID = createTask("Time",		// task name
		timeTask,	// task
		10,			// task priority
		argc,			// task arguments
		argv);
	return 0;
} // end P3_tdc


