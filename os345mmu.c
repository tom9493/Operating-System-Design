// os345mmu.c - LC-3 Memory Management Unit	06/21/2020
//
//		03/12/2015	added PAGE_GET_SIZE to accessPage()
//
// **************************************************************************
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
#include <assert.h>
#include "os345.h"
#include "os345lc3.h"

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;						// memory accesses
int memHits;						// memory hits
int memPageFaults;					// memory faults
int clockRPT;						// RPT clock
int clockUPT;						// UPT clock
int defined;

int getFrame(int);
int getAvailableFrame(void);
extern TCB tcb[];					// task control block
extern int curTask;					// current task #

int getFrame(int notme)
{
	int frame;
	int frameFound;
	

	frame = getAvailableFrame();
	if (frame >=0) return frame;

	// run clock
	frameFound = 0;

	if (clockRPT < LC3_RPT || clockRPT > LC3_RPT_END) { clockRPT = LC3_RPT; }

	while (1)
	{
		if (DEFINED(memory[clockRPT]))																// If RPT is defined
		{
			if (REFERENCED(memory[clockRPT])) { memory[clockRPT] = CLEAR_REF(memory[clockRPT]); }	// Clears referenced bit
			else																					// And is not referenced recently
			{
				if (clockUPT >= (FRAME(memory[clockRPT]) << 6) + 64 || clockUPT < FRAME(memory[clockRPT]) << 6)
				{
					clockUPT = FRAME(memory[clockRPT]) << 6;
					defined = 0;
				}						
				while (1)
				{
					//printf("clockUPT: 0x%x\n", clockUPT);
					if (DEFINED(memory[clockUPT]) && FRAME(memory[clockUPT]) != notme)				// If UPT is defined and != not me
					{
						if (REFERENCED(memory[clockUPT])) { memory[clockUPT] = CLEAR_REF(memory[clockUPT]); }		// If UPT frame is referenced, unset reference bit and increment
						else
						{
							if (DIRTY(memory[clockUPT]))
							{
								if (PAGED(memory[clockUPT + 1]))
								{
									memory[clockUPT + 1] |= accessPage(SWAPPAGE(memory[clockUPT + 1]), FRAME(memory[clockUPT]), PAGE_OLD_WRITE);
								}
								else
								{
									memory[clockUPT + 1] |= accessPage(SWAPPAGE(memory[clockUPT + 1]), FRAME(memory[clockUPT]), PAGE_NEW_WRITE);
									memory[clockUPT + 1] = SET_PAGED(memory[clockUPT + 1]);
								}
								memory[clockUPT] = CLEAR_DIRTY(memory[clockUPT]);
							}
							frame = FRAME(memory[clockUPT]);
							memory[clockUPT] = CLEAR_DEFINED(memory[clockUPT]);
							memory[clockUPT] = 0;
							frameFound = 1;
						}
					}
					if (DEFINED(memory[clockUPT])) { defined = 1; }
					clockUPT += 2;											// Increments to next entry (each is two words, so plus 2)
					if (clockUPT == (FRAME(memory[clockRPT]) << 6) + 64) 
					{ 
						clockUPT = 0; 
						if (!defined && frameFound == 0 && FRAME(memory[clockRPT]) != notme)
						{						// If no upt entries are defined, swap upt 
							//printf("\nclockRPT: 0x%x", clockRPT);
							if (DIRTY(memory[clockRPT]))
							{
								if (PAGED(memory[clockRPT + 1]))
								{
									memory[clockRPT + 1] |= accessPage(SWAPPAGE(memory[clockRPT + 1]), FRAME(memory[clockRPT]), PAGE_OLD_WRITE);
								}
								else
								{
									memory[clockRPT + 1] |= accessPage(SWAPPAGE(memory[clockRPT + 1]), FRAME(memory[clockRPT]), PAGE_NEW_WRITE);
									memory[clockRPT + 1] = SET_PAGED(memory[clockRPT + 1]);
								}
								memory[clockRPT] = CLEAR_DIRTY(memory[clockRPT]);
							}
							frame = FRAME(memory[clockRPT]);
							memory[clockRPT] = CLEAR_DEFINED(memory[clockRPT]);
							memory[clockRPT] = 0;
							frameFound = 1;
							clockRPT += 2;																				// Increments to next entry (each is two words, so plus 2)
							if (clockRPT >= LC3_RPT_END) { clockRPT = LC3_RPT; }										// Loops back around to the start of the RPTs
						}
						break;
					}
					if (frameFound) { break; }
				}
			}
		}
		if (frameFound) { break; }																	// If frame was found, leave both while loops
		clockRPT += 2;																				// Increments to next entry (each is two words, so plus 2)
		if (clockRPT >= LC3_RPT_END) { clockRPT = LC3_RPT; }										// Loops back around to the start of the RPTs
	}

	return frame;
}
// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	              / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p

#define MMU_ENABLE	1

unsigned short int *getMemAdr(int va, int rwFlg)
{
#if !MMU_ENABLE
	return &memory[va];
#else
	unsigned short int pa;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int rptFrame, uptFrame;

	// turn off virtual addressing for system RAM
	if (va < 0x3000) return &memory[va];
	// tcb[curTask].RPT is a pointer to the beginning of a root page table. The RPTI define will
	// take the virtual address and extract the bits that increment through the root page table
	// and add it to the pointer. rpta is now pointing to the correct root page table entry.
	// Because rpte1 and rpte2 are ints (16 bits/4 bytes), we need them both to represent the
	// 32-bit page table entry. 
	rpta = tcb[curTask].RPT + RPTI(va);							// root page table address

	rpte1 = memory[rpta];										// FDRP__ffffffffff
	rpte2 = memory[rpta+1];										// S___pppppppppppp
	memAccess++;
	memHits++;

	if (DEFINED(rpte1)) { memHits++; }							// rpte defined (Defined if the referenced frame is in main memory)				
	else														// rpte undefined
	{ 
		memPageFaults++;
		rpte1 = SET_DEFINED(getFrame(-1));						// Get frame in main memory we can put the page
		//printf("\nframe of rpte1: %d", FRAME(rpte1));
		if (PAGED(rpte2))										// If referenced page is in the swap space
		{
			//printf("\nWENT THROUGH"); fflush(stdout);
			accessPage(SWAPPAGE(rpte2), FRAME(rpte1), PAGE_READ); // SWAPPAGE gets the page bits from rpte2, takes specified page
		}														// and moves it from swap space into frame in main memory
		else													
		{
			rpte1 = SET_DIRTY(rpte1);
			rpte2 = 0;
			memset(&memory[FRAME(rpte1) << 6], 0, 64 * sizeof(memory[0]));	// Beginning of UPT is RPT frame * 64, or FRAME() << 6
		}
	}					
	memory[rpta] = rpte1 = SET_REF(SET_PINNED(rpte1));			// set rpt frame access bit
	memory[rpta + 1] = rpte2;

	upta = (FRAME(rpte1)<<6) + UPTI(va);						// user page table address
	upte1 = memory[upta]; 										// FDRP__ffffffffff
	upte2 = memory[upta+1]; 									// S___pppppppppppp
	memAccess++;

	if (DEFINED(upte1)) { memHits++; }							// upte defined					
	else														// upte undefined
	{ 
		memPageFaults++;
		upte1 = SET_DEFINED(getFrame(FRAME(rpte1)));
		//printf("\nframe of upte1: %d", FRAME(upte1));
		if (PAGED(upte2)) { accessPage(SWAPPAGE(upte2), FRAME(upte1), PAGE_READ); }
		else
		{
			upte1 = SET_DIRTY(upte1);
			upte2 = 0;
		}
	}					
	memory[upta] = upte1 = SET_REF(upte1); 			// set upt frame access bit
	memory[upta + 1] = upte2;
	memAccess++;
	if (rwFlg)
	{
		memory[rpta] = SET_DIRTY(rpte1);
		memory[upta] = SET_DIRTY(upte1);
	}
	if (&memory[(FRAME(upte1) << 6) + FRAMEOFFSET(va)] == 0) { printf("was zero\n"); }
	return &memory[(FRAME(upte1)<<6) + FRAMEOFFSET(va)];
#endif
} // end getMemAdr


// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef)
{	int i, data;
	int adr = LC3_FBT-1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i=0; i<LC3_FRAMES; i++)
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;
			adr++;
			data = (flg)?MEMWORD(adr):0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
		if ( (i >= sf) && (i < ef)) data = data | fmask;
		MEMWORD(adr) = data;
	}
	return;
} // end setFrameTableBits


// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame()
{
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i=0; i<LC3_FRAMES; i++)		// look thru all frames
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
		// deallocate frame and return frame #
		if (data & fmask)
		{  MEMWORD(adr) = data & ~fmask;
			return i;
		}
	}
	return -1;
} // end getAvailableFrame



// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg)
{
	static int nextPage;						// swap page size
	static int pageReads;						// page reads
	static int pageWrites;						// page writes
	static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

	if ((nextPage >= LC3_MAX_PAGE) || (pnum >= LC3_MAX_PAGE))
	{
		printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
		exit(-4);
	}
	switch(rwnFlg)
	{
		case PAGE_INIT:                    		// init paging
			clockRPT = 0;						// clear RPT clock
			clockUPT = 0;						// clear UPT clock
			memAccess = 0;						// memory accesses
			memHits = 0;						// memory hits
			memPageFaults = 0;					// memory faults
			nextPage = 0;						// disk swap space size
			pageReads = 0;						// disk page reads
			pageWrites = 0;						// disk page writes
			return 0;

		case PAGE_GET_SIZE:                    	// return swap size
			return nextPage;

		case PAGE_GET_READS:                   	// return swap reads
			return pageReads;

		case PAGE_GET_WRITES:                    // return swap writes
			return pageWrites;

		case PAGE_GET_ADR:                    	// return page address
			return (int)(&swapMemory[pnum<<6]);

		case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
			pnum = nextPage++;

		case PAGE_OLD_WRITE:                   // write
			//printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
			memcpy(&swapMemory[pnum<<6], &memory[frame<<6], 1<<7);
			pageWrites++;
			return pnum;

		case PAGE_READ:                    	// read
			//printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
			memcpy(&memory[frame<<6], &swapMemory[pnum<<6], 1<<7);
			pageReads++;
			return pnum;

		case PAGE_FREE:                   // free page
			printf("\nPAGE_FREE not implemented");
			break;
   }
   return pnum;
} // end accessPage
