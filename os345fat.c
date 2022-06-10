// os345fat.c - file management system	06/21/2020
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
//
//		11/19/2011	moved getNextDirEntry to P6
//
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345fat.h"

// ***********************************************************************
// ***********************************************************************
//	functions to implement in Project 6
//
int fmsCloseFile(int);
int fmsDefineFile(char*, int);
int fmsDeleteFile(char*);
int fmsOpenFile(char*, int);
int fmsReadFile(int, char*, int);
int fmsSeekFile(int, int);
int fmsWriteFile(int, char*, int);

// ***********************************************************************
// ***********************************************************************
//	Support functions available in os345p6.c
//
extern int fmsGetDirEntry(char* fileName, DirEntry* dirEntry);
extern int fmsGetNextDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir);

extern int fmsMount(char* fileName, void* ramDisk);

extern void setFatEntry(int FATindex, unsigned short FAT12ClusEntryVal, unsigned char* FAT);
extern unsigned short getFatEntry(int FATindex, unsigned char* FATtable);

extern int fmsMask(char* mask, char* name, char* ext);
extern void setDirTimeDate(DirEntry* dir);
extern int isValidFileName(char* fileName);
extern void printDirectoryEntry(DirEntry*);
extern void fmsError(int);

extern int fmsReadSector(void* buffer, int sectorNumber);
extern int fmsWriteSector(void* buffer, int sectorNumber);

// ***********************************************************************
// ***********************************************************************
// fms variables
//
// RAM disk
unsigned char RAMDisk[SECTORS_PER_DISK * BYTES_PER_SECTOR];

// File Allocation Tables (FAT1 & FAT2)
unsigned char FAT1[NUM_FAT_SECTORS * BYTES_PER_SECTOR];
unsigned char FAT2[NUM_FAT_SECTORS * BYTES_PER_SECTOR];

char dirPath[128];							// current directory path
FCB OFTable[NFILES];						// open file table

extern bool diskMounted;					// disk has been mounted
extern TCB tcb[];							// task control block
extern int curTask;							// current task #

int numOpenFiles = 0;


// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	int error;
	char buffer[BYTES_PER_SECTOR];
	if (!diskMounted) return ERR72;		// Check if disk is mounted
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) return ERR52;
	if (OFTable[fileDescriptor].name[0] == 0) return ERR63;

	// C_2_S changes cluster number to sector number

	if (OFTable[fileDescriptor].flags & FILE_ALTERED)
	{
		// save back to disk sector
		int sectorNumber = C_2_S(OFTable[fileDescriptor].currentCluster);
		if ((error = fmsWriteSector(&OFTable[fileDescriptor].buffer, sectorNumber)) < 0) return error;

		// Get sector data fram the directory cluster sector
		sectorNumber = C_2_S(OFTable[fileDescriptor].directoryCluster);
		if ((error = fmsReadSector(&buffer, sectorNumber)) < 0) return error;		// Reading a sector from memory into the buffer

		DirEntry dirEntry;

		// Update dirEntry corresponding to this
		if ((error = fmsGetDirEntry(OFTable[fileDescriptor].name, &dirEntry)) < 0) return error;

		// Update date time 
		setDirTimeDate(&dirEntry);

		// Update file size
		dirEntry.fileSize = OFTable[fileDescriptor].fileSize;
		// Get dir entry address and save dir entry
		DirEntry tempDirEntry;
		for (int i = 0; i < ENTRIES_PER_SECTOR; ++i)
		{
			memcpy(&tempDirEntry, &buffer[i * sizeof(DirEntry)], sizeof(DirEntry));		// Go through each entry per sector and copy the buffer of that directory and multiply it by the directory entry a

			if (tempDirEntry.name[0] == 0) return ERR67;				// EOD, why does it return?
			if (tempDirEntry.name[0] == 0xe5);
			else if (tempDirEntry.attributes == LONGNAME);
			else if (fmsMask(OFTable[fileDescriptor].name, tempDirEntry.name, tempDirEntry.extension))	// Matches name. Makes sure OFTable thing matches the name and extension?
			{
				memcpy(&buffer[i * sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));		// This is writing the OFT entry back to the buffer, which will write back into memory (after you found the matching name and extension)
				break;
			}
		}

		// Write back to disk sector
		if ((error = fmsWriteSector(&buffer, sectorNumber)) < 0) return error;			// Writes the changed buffer back to memory
	}
	// Set first bit to zero		
	numOpenFiles--;									// With the file closing, there are 1 fewer files open
	OFTable[fileDescriptor].name[0] = 0;			// Indicates this OFTable entry is undefined
	return 0;
} // end fmsCloseFile



// ***********************************************************************
// ***********************************************************************
// If attribute=DIRECTORY, this function creates a new directory
// file directoryName in the current directory.
// The directory entries "." and ".." are also defined.
// It is an error to try and create a directory that already exists.
//
// else, this function creates a new file fileName in the current directory.
// It is an error to try and create a file that already exists.
// The start cluster field should be initialized to cluster 0.  In FAT-12,
// files of size 0 should point to cluster 0 (otherwise chkdsk should report an error).
// Remember to change the start cluster field from 0 to a free cluster when writing to the
// file.
//
// Return 0 for success, otherwise, return the error number.
//
int fmsDefineFile(char* fileName, int attribute)
{
	if (!diskMounted) return ERR72;		// Check if disk is mounted
	// ?? add code here
	printf("\nfmsDefineFile Not Implemented");

	return ERR72;
} // end fmsDefineFile



// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current director.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName)
{
	if (!diskMounted) return ERR72;		// Check if disk is mounted
	// ?? add code here
	printf("\nfmsDeleteFile Not Implemented");

	return ERR61;
} // end fmsDeleteFile



// ***********************************************************************
// ***********************************************************************
// This function opens the file fileName for access as specified by rwMode.
// It is an error to try to open a file that does not exist.
// The open mode rwMode is defined as follows:
//    0 - Read access only.
//       The file pointer is initialized to the beginning of the file.
//       Writing to this file is not allowed.
//    1 - Write access only.
//       The file pointer is initialized to the beginning of the file.
//       Reading from this file is not allowed.
//    2 - Append access.
//       The file pointer is moved to the end of the file.
//       Reading from this file is not allowed.
//    3 - Read/Write access.
//       The file pointer is initialized to the beginning of the file.
//       Both read and writing to the file is allowed.
// A maximum of 32 files may be open at any one time.
// If successful, return a file descriptor that is used in calling subsequent file
// handling functions; otherwise, return the error number.
//
int fmsOpenFile(char* fileName, int rwMode)
{
	FCB newFCB;
	DirEntry dirEntry;
	int error;

	// Error checking
	if (!diskMounted) return ERR72;														// Check if disk is mounted
	if (isValidFileName(fileName) < 1) return ERR50;									// Invald file name
	if (numOpenFiles >= 32) return ERR70;												// Too many files open
	if ((error = fmsGetDirEntry(fileName, &dirEntry)) < 0) return error;					// File not defined, or file space full? Probs not

	// Make sure file is not already open
	for (int i = 0; i < NFILES; ++i)
	{
		//printf("OFTable[%d].name -- dirEntry.name: %s -- %s\n", i, OFTable[i].name, dirEntry.name);
		if (strncmp(OFTable[i].name, dirEntry.name, 11) == 0) { return ERR62; }				// strncmp returns 0 if same
	}

	memcpy(newFCB.name, dirEntry.name, 8);
	memcpy(newFCB.extension, dirEntry.extension, 3);									// Memcpy so not null terminated
	newFCB.attributes = dirEntry.attributes;
	newFCB.directoryCluster = CDIR;
	newFCB.startCluster = dirEntry.startCluster;
	newFCB.currentCluster = 0;

	if (rwMode == 1) { newFCB.fileSize = 0; }
	else { newFCB.fileSize = dirEntry.fileSize; }

	newFCB.pid = curTask;
	newFCB.mode = rwMode;
	newFCB.flags = 0;

	if (rwMode != 2) { newFCB.fileIndex = 0; }
	else { newFCB.fileIndex = dirEntry.fileSize; }	

	// Find available slot to insert file into OFTable
	for (int i = 0; i < NFILES; ++i)
	{
		if ((OFTable[i].name[0] == 0x00) || (OFTable[i].name[0] == 0xe5)) { OFTable[i] = newFCB; numOpenFiles++; return i; }
	}

	return ERR65;
} // end fmsOpenFile



// ***********************************************************************
// ***********************************************************************
// This function reads nBytes bytes from the open file specified by fileDescriptor into
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// After each read, the file pointer is advanced.
// Return the number of bytes successfully read (if > 0) or return an error number.
// (If you are already at the end of the file, return EOF error.  ie. you should never
// return a 0.)
//
int fmsReadFile(int fileDescriptor, char* buffer, int nBytes)
{
	if (!diskMounted) return ERR72;		// Check if disk is mounted, should be in every function
	// If file descriptor is in boundaries
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) { return ERR52; }
	int error, nextCluster;
	FCB* entry;
	int numBytesRead = 0;
	unsigned int bytesLeft, bufferIndex;
	entry = &OFTable[fileDescriptor];							// 
	if (entry->name[0] == 0) return ERR63;						// File not open
	if ((entry->mode == 1) || (entry->mode == 2)) return ERR85; // Incorrect mode
	while (nBytes > 0)
	{
		if (entry->fileSize == entry->fileIndex) return (numBytesRead ? numBytesRead : ERR66); // Read fewer than requested or none (EOF)
		bufferIndex = entry->fileIndex % BYTES_PER_SECTOR;
		if ((bufferIndex == 0) && (entry->fileIndex || !entry->currentCluster))
		{
			if (entry->currentCluster == 0)
			{
				if (entry->startCluster == 0) return ERR66;
				nextCluster = entry->startCluster;
				entry->fileIndex = 0;
			}
			else
			{
				nextCluster = getFatEntry(entry->currentCluster, FAT1);
				if (nextCluster == FAT_EOC) return numBytesRead;
			}
			if (entry->flags & BUFFER_ALTERED)
			{
				if ((error = fmsWriteSector(entry->buffer, C_2_S(entry->currentCluster)))) return error;
				entry->flags &= ~BUFFER_ALTERED;
			}
			entry->currentCluster = nextCluster;
			if ((error = fmsReadSector(entry->buffer, C_2_S(entry->currentCluster)))) return error;
		}
		bytesLeft = BYTES_PER_SECTOR - bufferIndex;
		if (bytesLeft > nBytes) bytesLeft = nBytes;
		if (bytesLeft > (entry->fileSize - entry->fileIndex)) bytesLeft = entry->fileSize - entry->fileIndex;
		memcpy(buffer, &entry->buffer[bufferIndex], bytesLeft);
		entry->fileIndex += bytesLeft;
		numBytesRead += bytesLeft;
		buffer += bytesLeft;
		nBytes -= bytesLeft;
	}
	return numBytesRead;
} // end fmsReadFile



// ***********************************************************************
// ***********************************************************************
// This function changes the current file pointer of the open file specified by
// fileDescriptor to the new file position specified by index.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// The file position may not be positioned beyond the end of the file.
// Return the new position in the file if successful; otherwise, return the error number.
//
int fmsSeekFile(int fileDescriptor, int index)
{
	if (!diskMounted) return ERR72;		// Check if disk is mounted
	// ?? add code here
	printf("\nfmsSeekFile Not Implemented");

	return ERR63;
} // end fmsSeekFile



// ***********************************************************************
// ***********************************************************************
// This function writes nBytes bytes to the open file specified by fileDescriptor from
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// Writing is always "overwriting" not "inserting" in the file and always writes forward
// from the current file pointer position.
// Return the number of bytes successfully written; otherwise, return the error number.
//
int fmsWriteFile(int fileDescriptor, char* buffer, int nBytes)
{
	if (!diskMounted) return ERR72;		// Check if disk is mounted
	// ?? add code here
	printf("\nfmsWriteFile Not Implemented");

	return ERR63;
} // end fmsWriteFile
