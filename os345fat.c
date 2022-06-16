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
extern int fmsGetDirEntry(char* fileName, DirEntry* entry);
extern int fmsGetNextDirEntry(int* dirNum, char* mask, DirEntry* entry, int dir);

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

int tableInsert(FCB entry)
{
	for (int i = 0; i < NFILES; i++)
	{
		if (OFTable[i].name[0] == 0)
		{
			OFTable[i] = entry;
			return i;
		}
	}
}

int getOFTSize()
{
	int count = 0;
	for (int i = 0; i < NFILES; i++)
	{
		if (OFTable[i].name[0] != 0) count++;
	}
	return count;
}

int isInOFT(DirEntry* entry)
{
	for (int i = 0; i < NFILES; i++)
	{
		if (!strncmp(OFTable[i].name, entry->name, 11)) return 1;
	}

	return 0;
}

// ***********************************************************************
// ***********************************************************************
//
//
void getFilename(char* filename, char* name, char* ext)
{
	int i;
	int flag = 0;
	int filenameLength = strlen(filename);
	for (i = 0; i < filenameLength; i++) filename[i] = toupper(filename[i]);
	for (i = 0; i < filenameLength; i++)
	{
		if (filename[i] == '.') break;
	}

	memcpy(name, filename, i);
	memcpy(&name[i], "         ", 8 - i);
	if (i != filenameLength)
	{
		int extlen = filenameLength - i - 1;
		memcpy(ext, &filename[i + 1], extlen);
		memcpy(&ext[extlen], "   ", 3 - extlen);
	}
	else
	{
		memcpy(ext, "   ", 3);
	}

}

// ***********************************************************************
// ***********************************************************************
//
//
char* findRightEntry(char* buffer, char* filename)
{
	for (int dirNum = 0; dirNum < ENTRIES_PER_SECTOR; dirNum++)
	{
		if (strncmp(buffer, filename, 11))
		{
			buffer += sizeof(DirEntry);
		}
		else return buffer;
	}
	return NULL;
}


// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	int error, root;
	char buffer[BYTES_PER_SECTOR];

	if (!diskMounted) return ERR72;
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) return ERR52;
	if (OFTable[fileDescriptor].name[0] == 0) return ERR63;
	
	if (OFTable[fileDescriptor].flags & FILE_ALTERED)							// Is file altered?
	{
		
		int sectorNumber = C_2_S(OFTable[fileDescriptor].currentCluster);		// Get memory sector and write back
		if (error = fmsWriteSector(&OFTable[fileDescriptor].buffer, sectorNumber)) return error;

		if (OFTable[fileDescriptor].directoryCluster == 0)						// Get sector data from the directory cluster sector.
		{
			sectorNumber = BEG_ROOT_SECTOR;
			root = 1;															// Directory its in is root
		}
		else
		{
			sectorNumber = C_2_S(OFTable[fileDescriptor].directoryCluster);
			root = 0;															// Parent directory not root
		}
		if (error = fmsReadSector(buffer, sectorNumber)) return error;

		char* bufIndex;
		while (1)
		{
			if ((bufIndex = findRightEntry(buffer, OFTable[fileDescriptor].name)) == NULL)
			{
				if (root) sectorNumber++;
				else sectorNumber = C_2_S(getFatEntry(S_2_C(sectorNumber), FAT1));
				if (error = fmsReadSector(buffer, sectorNumber)) return error;
			}
			else
			{
				((DirEntry*)bufIndex)->fileSize = OFTable[fileDescriptor].fileSize;			// update date time
				((DirEntry*)bufIndex)->startCluster = OFTable[fileDescriptor].startCluster;	// update start cluster
				setDirTimeDate(((DirEntry*)bufIndex));										// set time and date
				if (error = fmsWriteSector(buffer, sectorNumber))return error;
				break;
			}
		}
	}
	OFTable[fileDescriptor].name[0] = 0;
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
	DirEntry entry;
	int dirNum;
	int error, sectorNumber, clusterNumber, nextCluster;
	int root;
	char buffer[BYTES_PER_SECTOR];
	char buf[BYTES_PER_SECTOR];
	
	if (!diskMounted) return ERR72;
	if (isValidFileName(fileName) < 1) return ERR50;
	if (!fmsGetDirEntry(fileName, &entry)) return ERR60;
	
	clusterNumber = CDIR;
	if (clusterNumber == 0)											// Find starting sector number according to the starting cluster
	{
		sectorNumber = 19;											// Sector of root directory
		root = 1;													// In root directory
	}
	else
	{
		sectorNumber = C_2_S(clusterNumber);
		root = 0;
	}
	
	while (1)														// Find an empty entry spot in the directory
	{
		if (error = fmsReadSector(buf, sectorNumber)) return error;	// Put sector into buf

		for (dirNum = 0; dirNum < ENTRIES_PER_SECTOR; dirNum++)		// Find space in sector to put new entry
		{
			if (buf[dirNum * sizeof(DirEntry)] == 0xe5) continue;	// Deleted entry
			if (buf[dirNum * sizeof(DirEntry)] == 0) break;			// Empty -- use this
		}
		
		if (dirNum == ENTRIES_PER_SECTOR)							// No empty spot in cluster, go to next
		{
			if (root) sectorNumber++;								// If root go to next
			else
			{
				clusterNumber = S_2_C(sectorNumber);
				nextCluster = getFatEntry(clusterNumber, FAT1);
				if (nextCluster == FAT_EOC)							// Get empty cluster
				{					
					int newCluster = 2;
					while (getFatEntry(newCluster, FAT1) != 0) newCluster++;
					setFatEntry(clusterNumber, newCluster, FAT1);	// Set new cluster to EOC
					setFatEntry(newCluster, FAT_EOC, FAT1);			// Set old FAT entry to new one we just found
					memcpy(FAT2, FAT1, 4608);						// Reflect in FAT2
					clusterNumber = newCluster;
					sectorNumber = C_2_S(clusterNumber);
					char buffer[BYTES_PER_SECTOR];
					memset(buffer, 0, BYTES_PER_SECTOR);
					if (error = fmsWriteSector(buffer, sectorNumber)) return error;
				}
				else sectorNumber = C_2_S(nextCluster);
			}
		}
		else break;
	}																// Space to write to found, now write to it for sub-directory or file

	if (attribute & DIRECTORY)										// Sub-directory
	{
		int cluster = 2;											// Find empty cluster
		while (getFatEntry(cluster, FAT1) != 0) cluster++;
		setFatEntry(cluster, FAT_EOC, FAT1);
		memcpy(FAT2, FAT1, 4608);

		memset(buffer, 0, BYTES_PER_SECTOR);						// Make . and .. directory entries in new directory
		DirEntry tempEntry;
		setDirTimeDate(&tempEntry);
		tempEntry.attributes = 0x10;
		tempEntry.fileSize = 0;
		tempEntry.startCluster = cluster;
		memcpy(tempEntry.name, ".       ", 8);
		memcpy(tempEntry.extension, "   ", 3);
		memcpy(buffer, &tempEntry, sizeof(DirEntry));
		tempEntry.startCluster = clusterNumber;
		memcpy(tempEntry.name, "..      ", 8);
		memcpy(tempEntry.extension, "   ", 3);
		memcpy(&buffer[sizeof(DirEntry)], &tempEntry, sizeof(DirEntry));	// . then .. right after
		int tempSector = C_2_S(cluster);
		if (error = fmsWriteSector(buffer, tempSector)) return error;		// Write back to disk
		
		getFilename(fileName, entry.name, entry.extension);					// Create directory
		entry.attributes = attribute;
		entry.startCluster = cluster;
		entry.fileSize = 0;
		setDirTimeDate(&entry);
		
		memcpy(&buf[dirNum * sizeof(DirEntry)], &entry, sizeof(DirEntry));	// Save entry to current directory.
		if (error = fmsWriteSector(buf, sectorNumber)) return error;
	}
	else																	// New file (not directory)
	{
		getFilename(fileName, entry.name, entry.extension);
		entry.attributes = attribute;
		entry.startCluster = 0;
		entry.fileSize = 0;
		setDirTimeDate(&entry);
		
		memcpy(&buf[dirNum * sizeof(DirEntry)], &entry, sizeof(DirEntry));	// Save entry to current directory.
		if (error = fmsWriteSector(buf, sectorNumber)) return error;
	}

	return 0;
} // end fmsDefineFile



// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current directory.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName)
{
	DirEntry entry;
	int error, fileDescriptor, currCluster, nextCluster, dirSector, dirCluster, root;
	char buffer[BYTES_PER_SECTOR];

	if (!diskMounted) return ERR72;											// Error checking
	if (isValidFileName(fileName) < 1) return ERR50;
	int index = 0;
	if ((error = fmsGetNextDirEntry(&index, fileName, &entry, CDIR)) < 0) return error;
	
	if (entry.attributes & DIRECTORY)										// If directory, check if there is entry in it
	{
		currCluster = entry.startCluster;
		if (entry.startCluster == 0)										// Find starting sector number according to the starting cluster
		{
			dirSector = 19;
			root = 1;
		}
		else
		{
			root = 0;
			dirSector = C_2_S(currCluster);
		}
		
		int dirNum;															// Check in the directory
		while (1)
		{
			if (error = fmsReadSector(buffer, dirSector)) return error;
			for (dirNum = 0; dirNum < ENTRIES_PER_SECTOR; dirNum++)
			{
				
				if (buffer[dirNum * sizeof(DirEntry)] == 0) break;							// Empty
				else if (buffer[dirNum * sizeof(DirEntry)] == 0x2e) continue;				// . and .. entry
				else if (buffer[dirNum * sizeof(DirEntry)] == 0xffffffe5) continue;			// Deleted entry
				else return ERR69;															// Cannot delete
			}
			if (dirNum == ENTRIES_PER_SECTOR)
			{
				if (root)
				{
					dirSector++;
					if (dirSector >= BEG_DATA_SECTOR) break;								// Get directory sector
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
	
	else if (isInOFT(&entry)) return ERR69;									// Is the file open?
	int loop = index / ENTRIES_PER_SECTOR;									// Get cluster based on index
	dirCluster = CDIR;
	if (dirCluster == 0)
	{
		dirSector = 19;
		while (loop--) dirSector++;
	}
	else
	{
		while (loop--)
		{
			nextCluster = getFatEntry(dirCluster, FAT1);
			if (nextCluster == FAT_EOC) break;
			dirCluster = nextCluster;
		}
		dirSector = C_2_S(dirCluster);
	}

	index = (index % ENTRIES_PER_SECTOR) - 1;
	entry.name[0] = 0xe5;
	if (error = fmsReadSector(buffer, dirSector)) return error;
	memcpy(&buffer[index * sizeof(DirEntry)], &entry, sizeof(DirEntry));
	if (error = fmsWriteSector(buffer, dirSector)) return error;

	currCluster = 0;
	nextCluster = entry.startCluster;
	if (entry.startCluster != 0)
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
	FCB fcb;
	DirEntry entry;
	int error;
	int fileDescriptor;

	if (!diskMounted) return ERR72;											// Error checking
	if (isValidFileName(fileName) < 1) return ERR50;
	if (getOFTSize() >= NFILES) return ERR65;
	if ((error = fmsGetDirEntry(fileName, &entry)) < 0) return error;
	if (isInOFT(&entry)) return ERR62;
		
	memcpy(fcb.name, entry.name, 8);										// Copy directory entry info into FCB entry
	memcpy(fcb.extension, entry.extension, 3);
	fcb.attributes = entry.attributes;
	fcb.directoryCluster = CDIR;
	fcb.startCluster = entry.startCluster;
	fcb.currentCluster = 0;
	fcb.fileSize = (rwMode == 1) ? 0 : entry.fileSize;
	fcb.pid = curTask;
	fcb.mode = rwMode;
	fcb.flags = 0;
	fcb.fileIndex = (rwMode != 2) ? 0 : entry.fileSize;
	
	fileDescriptor = tableInsert(fcb);										// Put FCB entry into the OFTable and return fd
	return fileDescriptor;
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
	int error, nextCluster;
	FCB* entry;
	int numBytesRead = 0;
	unsigned int bytesLeft, bufIndex;

	if (!diskMounted) return ERR72;												// Error checks
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) return ERR52;

	entry = &OFTable[fileDescriptor];
	if (entry->name[0] == 0) return ERR63;
	if ((entry->mode == 1) || (entry->mode == 2)) return ERR85;

	while (nBytes > 0)	
	{
		if (entry->fileSize == entry->fileIndex) return (numBytesRead ? numBytesRead : ERR66);	// At end of file, if read nothing EOF. Else return numBytesRead
		bufIndex = entry->fileIndex % BYTES_PER_SECTOR;											// Get bufIndex relative to current cluster
		if ((bufIndex == 0) && (entry->fileIndex || !entry->currentCluster))					// If at the end of a cluster, or starting, get cluster
		{
			if (entry->currentCluster == 0)														// Just starting
			{
				if (entry->startCluster == 0) return ERR66;										// File empty
				nextCluster = entry->startCluster;												// Set cluster to startCluster, reading from start of file
				entry->fileIndex = 0;
			}
			else
			{
				nextCluster = getFatEntry(entry->currentCluster, FAT1);							// End of previous, continuing to next cluster
				if (nextCluster == FAT_EOC) return numBytesRead;								// If end of file, return numBytesRead
			}

			if (entry->flags & BUFFER_ALTERED)													// If buf was altered, need to write back to sector
			{
				if (error = fmsWriteSector(entry->buffer, C_2_S(entry->currentCluster))) return error;
				entry->flags &= ~BUFFER_ALTERED;
			}

			entry->currentCluster = nextCluster;												// Set next cluster to entry's current cluster
			if (error = fmsReadSector(entry->buffer, C_2_S(entry->currentCluster))) return error;	// Read from current cluster, matching sector in memory
		}
		bytesLeft = BYTES_PER_SECTOR - bufIndex;												// Read bytes into buffer and print after, adjust byte values
		if (bytesLeft > nBytes) bytesLeft = nBytes;
		if (bytesLeft > (entry->fileSize - entry->fileIndex)) bytesLeft = entry->fileSize - entry->fileIndex;
		memcpy(buffer, &entry->buffer[bufIndex], bytesLeft);
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
	int error, nextCluster;
	FCB* entry;
	unsigned int bytesLeft, bufIndex;
	
	if (!diskMounted) return ERR72;													// Error checks
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) return ERR52;
	entry = &OFTable[fileDescriptor];
	if (index >= entry->fileSize) return ERR80;

	int i = index / BYTES_PER_SECTOR;										// Use this to find the cluster we're looking for
	entry->currentCluster = entry->startCluster;							// Start at the start cluster
	while ((i--) > 0)														// Traverse to the needed cluster and set the entries current cluster
	{
		nextCluster = getFatEntry(entry->currentCluster, FAT1);
		entry->currentCluster = nextCluster;
	}										
	if (error = fmsReadSector(&entry->buffer, C_2_S(entry->currentCluster))) return error; // Read from memory so entry's buf reflects current sector

	entry->fileIndex = index;												// Apply index given
	return index;
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
	int error, nextCluster;
	FCB* entry;
	int numBytesWritten = 0;
	unsigned int bytesLeft, bufIndex;
																		// Error checks
	if (!diskMounted) return ERR72;
	if ((fileDescriptor >= NFILES) || (fileDescriptor < 0)) return ERR52;
	entry = &OFTable[fileDescriptor];
	if (entry->name[0] == 0) return ERR63;
	if (entry->attributes == READ_ONLY) return ERR84;
	if ((entry->mode != 1) && (entry->mode != 3)) return ERR85;
	
	if (entry->startCluster == 0)										// If empty file give cluster
	{
		int cluster = 2;
		while (getFatEntry(cluster, FAT1) != 0) cluster++;
		entry->startCluster = cluster;
		setFatEntry(entry->startCluster, FAT_EOC, FAT1);
		memcpy(FAT2, FAT1, 4608);
	}

	while (nBytes > 0)													// Start writing bytes
	{
		bufIndex = entry->fileIndex % BYTES_PER_SECTOR;					// Get bufIndex in current sector
		if ((bufIndex == 0) && (entry->fileIndex || !entry->currentCluster))	// If at the start of cluster
		{
			if (entry->currentCluster == 0)								// currentCluster not set
			{
				if (entry->startCluster == 0) return ERR66;				// startCluster set to 0 if at end of file
				nextCluster = entry->startCluster;						// get next cluster to write to
				entry->fileIndex = 0;									// fileIndex at start of file
			}
			else
			{
				nextCluster = getFatEntry(entry->currentCluster, FAT1);	// get next cluster through FAT table
				if (nextCluster == FAT_EOC)								// if next FAT entry is EOC, need to 
				{
					int cluster = 2;
					while (getFatEntry(cluster, FAT1) != 0) cluster++;	// Find available cluster to write to
					setFatEntry(entry->currentCluster, cluster, FAT1);	// Adjust fat tables
					setFatEntry(cluster, FAT_EOC, FAT1);
					memcpy(FAT2, FAT1, 4608);
					nextCluster = cluster;								// Cluster to write to is cluster just found
				}
			}
			if (entry->flags & BUFFER_ALTERED)							// Before writing, if buf was altered, write to sector first
			{
				if (error = fmsWriteSector(entry->buffer, C_2_S(entry->currentCluster))) return error;
				entry->flags &= ~BUFFER_ALTERED;
			}

			entry->currentCluster = nextCluster;						// Now apply nextCluster
			if (error = fmsReadSector(entry->buffer, C_2_S(nextCluster))) return error;
		}
		bytesLeft = BYTES_PER_SECTOR - bufIndex;						// Write to buffer and indicate altered
		if (bytesLeft > nBytes) bytesLeft = nBytes;

		memcpy(&entry->buffer[bufIndex], buffer, bytesLeft);
		entry->flags |= BUFFER_ALTERED;
		entry->flags |= FILE_ALTERED;

		entry->fileIndex += bytesLeft;
		if (entry->fileIndex > entry->fileSize) entry->fileSize = entry->fileIndex;
		numBytesWritten += bytesLeft;
		buffer += bytesLeft;
		nBytes -= bytesLeft;
	}

	return numBytesWritten;
} // end fmsWriteFile