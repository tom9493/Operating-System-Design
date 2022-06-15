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

void getFilename(char* filename, char* name, char* extension)
{
	int i;
	int flag = 0;
	int length = strlen(filename);
	for (i = 0; i < length; i++) filename[i] = toupper(filename[i]);
	for (i = 0; i < length; i++)
	{
		if (filename[i] == '.') break;
	}

	memcpy(name, filename, i);
	memcpy(&name[i], "         ", 8 - i);
	if (i != length)
	{
		int extensionLen = length - i - 1;
		memcpy(extension, &filename[i + 1], extensionLen);
		memcpy(&extension[extensionLen], "   ", 3 - extensionLen);
	}
	else
	{
		memcpy(extension, "   ", 3);
	}
}

// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	int error, root;
	char buffer[BYTES_PER_SECTOR];
	if (!diskMounted) return ERR72;											// Check if disk is mounted
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) return ERR52;	// Check invalid file descriptor
	if (OFTable[fileDescriptor].name[0] == 0) return ERR63;					// 

	// C_2_S changes cluster number to sector number

	if (OFTable[fileDescriptor].flags & FILE_ALTERED)
	{
		// save back to disk sector
		int sectorNumber = C_2_S(OFTable[fileDescriptor].currentCluster);
		if (error = fmsWriteSector(&OFTable[fileDescriptor].buffer, sectorNumber)) return error;

		if (OFTable[fileDescriptor].directoryCluster == 0)
		{
			sectorNumber = BEG_DATA_SECTOR;
			root = 1;
		}
		else
		{
			sectorNumber = C_2_S(OFTable[fileDescriptor].directoryCluster);
			root = 0;
		}
		
		if (error = fmsReadSector(buffer, sectorNumber)) return error;

		char* bufIndex = NULL;
		char* tempBuf = buffer;
		while (1)
		{
			for (int i = 0; i < ENTRIES_PER_SECTOR; i++)
			{
				if (strncmp(tempBuf, OFTable[fileDescriptor].name, 11))
				{
					tempBuf += sizeof(DirEntry);
				}
				else
				{
					bufIndex = tempBuf; 
					break;
				}
				if (i == ENTRIES_PER_SECTOR - 1) bufIndex == NULL;
			}
			if (bufIndex == NULL)
			{
				if (root) sectorNumber++;
				else sectorNumber = C_2_S(getFatEntry(S_2_C(sectorNumber), FAT1));
				if (error = fmsReadSector(buffer, sectorNumber)) return error;
			}
			else
			{
				((DirEntry*)bufIndex)->fileSize = OFTable[fileDescriptor].fileSize;
				((DirEntry*)bufIndex)->startCluster = OFTable[fileDescriptor].startCluster;
				setDirTimeDate(((DirEntry*)bufIndex));
				if (error = fmsWriteSector(buffer, sectorNumber)) return error;
				break;
			}
		}

		
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
	int error, i, sectorNum, clusterNum, nextCluster, root;
	char buffer[BYTES_PER_SECTOR], currentDirBuf[BYTES_PER_SECTOR];
	DirEntry dirEntry;

	if (!diskMounted) return ERR72;							// Check if disk is mounted
	if (isValidFileName(fileName) < 1) return ERR50;
	error = fmsGetDirEntry(fileName, &dirEntry);
	if (error != ERR61) return ERR60;						// If this doesn't return "file not defined", the file is already defined, which is wrong. Return "file already defined".

	clusterNum = CDIR;
	if (clusterNum == 0)
	{
		sectorNum = 19;
		root = 1;
	}
	else
	{
		sectorNum = C_2_S(clusterNum);
		root = 0;
	}

	while (1)
	{
		if (error = fmsReadSector(currentDirBuf, sectorNum)) return error;
		for (i = 0; i < ENTRIES_PER_SECTOR; i++)
		{
			if (currentDirBuf[i * sizeof(DirEntry)] == 0xe5) continue;
			if (currentDirBuf[i * sizeof(DirEntry)] == 0) break;
		}
		if (i == ENTRIES_PER_SECTOR)
		{
			if (root) sectorNum++;
			else
			{
				clusterNum = S_2_C(sectorNum);
				nextCluster = getFatEntry(clusterNum, FAT1);
				if (nextCluster == FAT_EOC)
				{
					int newCluster = 2;
					while (getFatEntry(newCluster, FAT1) != 0) newCluster++;
					setFatEntry(clusterNum, newCluster, FAT1);
					setFatEntry(newCluster, FAT_EOC, FAT1);
					memcpy(FAT2, FAT1, 4608);
					clusterNum = newCluster;
					sectorNum = C_2_S(clusterNum);
					// new buf here for errors below?
					memset(buffer, 0, BYTES_PER_SECTOR);
					if (error = fmsWriteSector(buffer, sectorNum)) return error;
				}
				else sectorNum = C_2_S(nextCluster);
			}
		}
		else break;
	}

	if (attribute & DIRECTORY)
	{
		DirEntry temp;

		int cluster = 2;
		while (getFatEntry(cluster, FAT1) != 0) cluster++;
		setFatEntry(cluster, FAT_EOC, FAT1);
		memcpy(FAT2, FAT1, 4608);

		memset(buffer, 0, BYTES_PER_SECTOR);
		setDirTimeDate(&temp);
		temp.attributes = 0x10;
		temp.fileSize = 0;
		temp.startCluster = cluster;
		memcpy(temp.name, ".       ", 8);
		memcpy(temp.extension, "   ", 3);
		memcpy(buffer, &temp, sizeof(DirEntry));
		
		temp.startCluster = clusterNum;
		memcpy(temp.name, "..      ", 8);
		memcpy(temp.extension, "   ", 3);
		memcpy(&buffer[sizeof(DirEntry)], &temp, sizeof(DirEntry));
		int sector = C_2_S(cluster);
		if (error = fmsWriteSector(buffer, sector)) return error;
		
		getFilename(fileName, dirEntry.name, dirEntry.extension);
		dirEntry.attributes = attribute;
		dirEntry.startCluster = cluster;
		dirEntry.fileSize = 0;
		setDirTimeDate(&dirEntry);
		memcpy(&currentDirBuf[i * sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));
		if (error = fmsWriteSector(currentDirBuf, sectorNum)) return error;
	}
	else
	{
		getFilename(fileName, dirEntry.name, dirEntry.extension);
		dirEntry.attributes = attribute;
		dirEntry.startCluster = 0;
		dirEntry.fileSize = 0;
		setDirTimeDate(&dirEntry);
		memcpy(&currentDirBuf[i * sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));
		if (error = fmsWriteSector(currentDirBuf, sectorNum)) return error;
	}

	return 0;
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
	DirEntry dirEntry;
	char buffer[BYTES_PER_SECTOR];
	int error, i, fileDescriptor, root, dirSector, dirCluster, currCluster, nextCluster, tempCluster;
	int index = 0;

	if (!diskMounted) return ERR72;															// Check if disk is mounted
	if (isValidFileName(fileName) < 1) return ERR50;										// Invald file name
	if ((error = fmsGetNextDirEntry(&index, fileName, &dirEntry, CDIR)) < 0) return error;	// File not defined, or file space full? Probs not

	if (dirEntry.attributes & DIRECTORY)
	{
		currCluster = dirEntry.startCluster;
		if (dirEntry.startCluster == 0)
		{
			dirSector = 19;
			root = 1;
		}
		else
		{
			root = 0;
			dirSector = C_2_S(currCluster);
		}
		while (1)
		{
			if (error = fmsReadSector(buffer, dirSector)) return error;
			for (i = 0; i < ENTRIES_PER_SECTOR; i++)
			{
				if (buffer[i * sizeof(DirEntry)] == 0) break;
				else if (buffer[i * sizeof(DirEntry)] == 0x2e) continue;
				else if (buffer[i * sizeof(DirEntry)] == 0xffffffe5) continue;    // Deleted entry
				else return ERR69;
			}
			if (i == ENTRIES_PER_SECTOR)
			{
				if (root)
				{
					dirSector++;
					if (dirSector >= BEG_DATA_SECTOR) break;
				}
				else
				{
					if (currCluster == FAT_EOC) break;
					currCluster = getFatEntry(S_2_C(dirSector), FAT1);
					dirSector = C_2_S(currCluster);
				}
			}
			else break;
		}
	}
	
	// Make sure file is not already open
	for (int i = 0; i < NFILES; ++i)
	{
		if (strncmp(OFTable[i].name, dirEntry.name, 11) == 0) { return ERR62; }			// strncmp returns 0 if same
	}

	int loop = index / ENTRIES_PER_SECTOR;
	dirCluster = CDIR;
	if (dirCluster == 0)
	{
		dirSector = 19;
		while(loop--) dirSector++;
	}
	else
	{
		while (loop--)
		{
			nextCluster = getFatEntry(dirCluster, FAT1);
			if (nextCluster == FAT_EOC) break;
			dirCluster = nextCluster;
		}
	}

	index = (index % ENTRIES_PER_SECTOR) - 1;
	dirEntry.name[0] = 0xe5;
	if (error = fmsReadSector(buffer, dirSector)) return error;
	memcpy(&buffer[index * sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));
	if (error = fmsWriteSector(buffer, dirSector)) return error;

	currCluster = 0;
	nextCluster = dirEntry.startCluster;
	if (dirEntry.startCluster != 0)
	{
		while (1)
		{
			currCluster = nextCluster;
			nextCluster = getFatEntry(currCluster, FAT1);
			setFatEntry(currCluster, 0, FAT1);
			if (currCluster == FAT_EOC) break;
		}
		memcpy(FAT2, FAT1, 4608);
	}
	return 0;
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
	if ((error = fmsGetDirEntry(fileName, &dirEntry)) < 0) return error;				// File not defined, or file space full? Probs not

	// Make sure file is not already open
	for (int i = 0; i < NFILES; ++i)
	{
		if (strncmp(OFTable[i].name, dirEntry.name, 11) == 0) { return ERR62; }			// strncmp returns 0 if same
	}
	
	memcpy(newFCB.name, dirEntry.name, 8);												// Copying all dirEntry info into the FCB
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
	if (!diskMounted) return ERR72;																// Check if disk is mounted, should be in every function
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) { return ERR52; }					// If file descriptor is in boundaries
	
	int error, nextCluster;
	FCB* entry;
	int numBytesRead = 0;
	unsigned int bytesLeft, bufferIndex;
	entry = &OFTable[fileDescriptor];															// Gets entry of OFTable 
	
	if (entry->name[0] == 0) return ERR63;														// File not open
	if ((entry->mode == 1) || (entry->mode == 2)) return ERR85;									// Incorrect mode
	while (nBytes > 0)
	{
		if (entry->fileSize == entry->fileIndex) return (numBytesRead ? numBytesRead : ERR66);	// Read fewer than requested or none (EOF)
		bufferIndex = entry->fileIndex % BYTES_PER_SECTOR;										// Buffer index is the number of bytes the file is reading into the current sector its in
		if ((bufferIndex == 0) && (entry->fileIndex || !entry->currentCluster))					// If we are switching to next cluster. Sets nextCluster
		{
			if (entry->currentCluster == 0)														// If we are at the start of the file
			{
				if (entry->startCluster == 0) return ERR66;										// startCluster gets set to 0 if this is an empty file. Define file will set this to right cluster
				nextCluster = entry->startCluster;												// If not empty file, set nextCluster to first cluster in file
				entry->fileIndex = 0;															// File index is also set to zero
			}
			else
			{
				nextCluster = getFatEntry(entry->currentCluster, FAT1);							// If not at start of file, use fat table to get next cluster
				if (nextCluster == FAT_EOC) return numBytesRead;								// return numBytes read if next cluster is marked as end of chain
			}
			if (entry->flags & BUFFER_ALTERED)													// Don't read if a write needs to be written back. Current cluster of file is only one that is in OFT?
			{
				if ((error = fmsWriteSector(entry->buffer, C_2_S(entry->currentCluster)))) return error;	// Writes back to memory
				entry->flags &= ~BUFFER_ALTERED;															// Takes away the BUFFER_ALTERED flag
			}
			entry->currentCluster = nextCluster;															// currentCluster in entry is set to nextCluster we are moving to
			if ((error = fmsReadSector(entry->buffer, C_2_S(entry->currentCluster)))) return error;			// Reads sector from memory and writes it into the entry buffer
		}
		bytesLeft = BYTES_PER_SECTOR - bufferIndex;												// Bytes left we can read from this cluster/sector
		if (bytesLeft > nBytes) bytesLeft = nBytes;												// Only read the number of bytes requested instead of the whole sector
		if (bytesLeft > (entry->fileSize - entry->fileIndex)) bytesLeft = entry->fileSize - entry->fileIndex;// Only read the amount of bytes left in the file instead of past that
		memcpy(buffer, &entry->buffer[bufferIndex], bytesLeft);									// Copy bytes from entry starting at buffer index, bytesLeft amount into the buffer given
		entry->fileIndex += bytesLeft;															// Set fileIndex up to where we stopped reading
		numBytesRead += bytesLeft;																// Give the number of bytes we were actually able to read regardless of request
		buffer += bytesLeft;																	// Increment buffer so next read appends to the end 
		nBytes -= bytesLeft;																	// "bytesLeft" fewer bytes to read, so decrease nBytes
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
