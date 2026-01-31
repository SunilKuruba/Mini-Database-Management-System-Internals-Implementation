#include<stdio.h>
#include<stdlib.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <math.h>

// This structure represents one page frame in buffer pool (memory).
typedef struct Page
{
	SM_PageHandle data; // Actual data of the page
	PageNumber pageNum; // An identification integer given to each page
	int dirtyBit; // Used to indicate whether the contents of the page has been modified by the client
	int fixCount; // Used to indicate the number of clients using that page at a given instance
	int hitNum;   // Used by LRU and clock algorithm to get the least recently used page	
	int refNum;   // Used by LFU algorithm to get the least frequently used page
} PageFrame;

// "bufferSize" represents the size of the buffer pool i.e. maximum number of page frames that can be kept into the buffer pool
int bufferSize = 0;

// "rearIndex" basically stores the count of number of pages read from the disk.
// "rearIndex" is also used by FIFO function to calculate the frontIndex i.e.
int rearIndex = 0;

// "writeCount" counts the number of I/O write to the disk i.e. number of pages writen to the disk
int writeCount = 0;

// "hit" a general count which is incremented whenever a page frame is added into the buffer pool.
// "hit" is used by LRU to determine least recently added page into the buffer pool.
int hit = 0;

// "clockPointer" is used by CLOCK algorithm to point to the last added page in the buffer pool.
int clockPointer = 0;

// "lfuPointer" is used by LFU algorithm to store the least frequently used page frame's position. It speeds up operation  from 2nd replacement onwards.
int lfuPointer = 0;

// LRU-K algorithm variables
#define K_VALUE 2  // K value for LRU-K algorithm (can be dynamic as per requirement)
#define MAX_HISTORY 100  // Maximum history entries per page

// Structure to store access history for LRU-K
typedef struct {
	PageNumber pageNum;
	int accessTimes[K_VALUE];  // Store last K access times
	int accessCount;  // Number of times the page has been accessed
} LRU_K_History;

LRU_K_History *lruKHistory = NULL;
int lruKTime = 0;  // Global time counter for LRU-K

// Defining FIFO (First In First Out) function
extern void FIFO(BM_BufferPool *const bm, PageFrame *page)
{
	//printf("FIFO Started");
	PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
	
	int i, frontIndex;
	frontIndex = rearIndex % bufferSize;

	// Interating through all the page frames in the buffer pool
	for(i = 0; i < bufferSize; i++)
	{
		if(pageFrame[frontIndex].fixCount == 0)
		{
			// If page in memory has been modified (dirtyBit = 1), then write page to disk
			if(pageFrame[frontIndex].dirtyBit == 1)
			{
				SM_FileHandle fh;
				openPageFile(bm->pageFile, &fh);
				writeBlock(pageFrame[frontIndex].pageNum, &fh, pageFrame[frontIndex].data);
				
				// Increase the writeCount which records the number of writes done by the buffer manager.
				writeCount++;
			}
			
			// Setting page frame's content to new page's content
			pageFrame[frontIndex].data = page->data;
			pageFrame[frontIndex].pageNum = page->pageNum;
			pageFrame[frontIndex].dirtyBit = page->dirtyBit;
			pageFrame[frontIndex].fixCount = page->fixCount;
			break;
		}
		else
		{
			// If the current page frame is being used by some client, we move on to the next location
			frontIndex++;
			frontIndex = (frontIndex % bufferSize == 0) ? 0 : frontIndex;
		}
	}
}

// Defining LFU (Least Frequently Used) function
extern void LFU(BM_BufferPool *const bm, PageFrame *page)
{
	//printf("LFU Started");
	PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
	
	int i, j, leastFreqIndex, leastFreqRef;
	leastFreqIndex = lfuPointer;	
	
	// Interating through all the page frames in the buffer pool
	for(i = 0; i < bufferSize; i++)
	{
		if(pageFrame[leastFreqIndex].fixCount == 0)
		{
			leastFreqIndex = (leastFreqIndex + i) % bufferSize;
			leastFreqRef = pageFrame[leastFreqIndex].refNum;
			break;
		}
	}

	i = (leastFreqIndex + 1) % bufferSize;

	// Finding the page frame having minimum refNum (i.e. it is used the least frequent) page frame
	for(j = 0; j < bufferSize; j++)
	{
		if(pageFrame[i].refNum < leastFreqRef)
		{
			leastFreqIndex = i;
			leastFreqRef = pageFrame[i].refNum;
		}
		i = (i + 1) % bufferSize;
	}
		
	// If page in memory has been modified (dirtyBit = 1), then write page to disk	
	if(pageFrame[leastFreqIndex].dirtyBit == 1)
	{
		SM_FileHandle fh;
		openPageFile(bm->pageFile, &fh);
		writeBlock(pageFrame[leastFreqIndex].pageNum, &fh, pageFrame[leastFreqIndex].data);
		
		// Increase the writeCount which records the number of writes done by the buffer manager.
		writeCount++;
	}
	
	// Setting page frame's content to new page's content		
	pageFrame[leastFreqIndex].data = page->data;
	pageFrame[leastFreqIndex].pageNum = page->pageNum;
	pageFrame[leastFreqIndex].dirtyBit = page->dirtyBit;
	pageFrame[leastFreqIndex].fixCount = page->fixCount;
	lfuPointer = leastFreqIndex + 1;
}

// Defining LRU (Least Recently Used) function
extern void LRU(BM_BufferPool *const bm, PageFrame *page)
{	
	PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
	int i, leastHitIndex, leastHitNum;

	// Interating through all the page frames in the buffer pool.
	for(i = 0; i < bufferSize; i++)
	{
		// Finding page frame whose fixCount = 0 i.e. no client is using that page frame.
		if(pageFrame[i].fixCount == 0)
		{
			leastHitIndex = i;
			leastHitNum = pageFrame[i].hitNum;
			break;
		}
	}	

	// Finding the page frame having minimum hitNum (i.e. it is the least recently used) page frame
	for(i = leastHitIndex + 1; i < bufferSize; i++)
	{
		if(pageFrame[i].hitNum < leastHitNum)
		{
			leastHitIndex = i;
			leastHitNum = pageFrame[i].hitNum;
		}
	}

	// If page in memory has been modified (dirtyBit = 1), then write page to disk
	if(pageFrame[leastHitIndex].dirtyBit == 1)
	{
		SM_FileHandle fh;
		openPageFile(bm->pageFile, &fh);
		writeBlock(pageFrame[leastHitIndex].pageNum, &fh, pageFrame[leastHitIndex].data);
		
		// Increase the writeCount which records the number of writes done by the buffer manager.
		writeCount++;
	}
	
	// Setting page frame's content to new page's content
	pageFrame[leastHitIndex].data = page->data;
	pageFrame[leastHitIndex].pageNum = page->pageNum;
	pageFrame[leastHitIndex].dirtyBit = page->dirtyBit;
	pageFrame[leastHitIndex].fixCount = page->fixCount;
	pageFrame[leastHitIndex].hitNum = page->hitNum;
}

// Defining CLOCK function
extern void CLOCK(BM_BufferPool *const bm, PageFrame *page)
{	
	//printf("CLOCK Started");
	PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
	while(1)
	{
		clockPointer = (clockPointer % bufferSize == 0) ? 0 : clockPointer;

		if(pageFrame[clockPointer].hitNum == 0)
		{
			// If page in memory has been modified (dirtyBit = 1), then write page to disk
			if(pageFrame[clockPointer].dirtyBit == 1)
			{
				SM_FileHandle fh;
				openPageFile(bm->pageFile, &fh);
				writeBlock(pageFrame[clockPointer].pageNum, &fh, pageFrame[clockPointer].data);
				
				// Increase the writeCount which records the number of writes done by the buffer manager.
				writeCount++;
			}
			
			// Setting page frame's content to new page's content
			pageFrame[clockPointer].data = page->data;
			pageFrame[clockPointer].pageNum = page->pageNum;
			pageFrame[clockPointer].dirtyBit = page->dirtyBit;
			pageFrame[clockPointer].fixCount = page->fixCount;
			pageFrame[clockPointer].hitNum = page->hitNum;
			clockPointer++;
			break;	
		}
		else
		{
			// Incrementing clockPointer so that we can check the next page frame location.
			// We set hitNum = 0 so that this loop doesn't go into an infinite loop.
			pageFrame[clockPointer++].hitNum = 0;		
		}
	}
}

// Defining LRU-K (Least Recently Used K) function
extern void LRU_K(BM_BufferPool *const bm, PageFrame *page)
{
	PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
	int i, victimIndex = -1;
	int maxBackwardKDistance = -1;
	
	// Find the page with the maximum backward K-distance
	for(i = 0; i < bufferSize; i++)
	{
		if(pageFrame[i].fixCount == 0)
		{
			int backwardKDistance = 0;
			
			// Find the history entry for this page
			int historyIdx = -1;
			for(int j = 0; j < bufferSize; j++)
			{
				if(lruKHistory[j].pageNum == pageFrame[i].pageNum)
				{
					historyIdx = j;
					break;
				}
			}
			
			if(historyIdx != -1 && lruKHistory[historyIdx].accessCount >= K_VALUE)
			{
				// Page has K or more accesses, use K-th most recent access time
				backwardKDistance = lruKTime - lruKHistory[historyIdx].accessTimes[K_VALUE - 1];
			}
			else
			{
				// Page has fewer than K accesses, use infinity (represented by a large number)
				backwardKDistance = 999999;
			}
			
			// Select the page with maximum backward K-distance
			if(victimIndex == -1 || backwardKDistance > maxBackwardKDistance)
			{
				victimIndex = i;
				maxBackwardKDistance = backwardKDistance;
			}
		}
	}
	
	if(victimIndex == -1)
		return;  // All pages are pinned
	
	// If page in memory has been modified (dirtyBit = 1), then write page to disk
	if(pageFrame[victimIndex].dirtyBit == 1)
	{
		SM_FileHandle fh;
		openPageFile(bm->pageFile, &fh);
		writeBlock(pageFrame[victimIndex].pageNum, &fh, pageFrame[victimIndex].data);
		
		// Increase the writeCount which records the number of writes done by the buffer manager.
		writeCount++;
	}
	
	// Setting page frame's content to new page's content
	pageFrame[victimIndex].data = page->data;
	pageFrame[victimIndex].pageNum = page->pageNum;
	pageFrame[victimIndex].dirtyBit = page->dirtyBit;
	pageFrame[victimIndex].fixCount = page->fixCount;
	
	// Update history for the new page
	int historyIdx = -1;
	for(int j = 0; j < bufferSize; j++)
	{
		if(lruKHistory[j].pageNum == page->pageNum || lruKHistory[j].pageNum == -1)
		{
			historyIdx = j;
			break;
		}
	}
	
	if(historyIdx != -1)
	{
		lruKHistory[historyIdx].pageNum = page->pageNum;
		
		// Shift access times and add new access
		for(int k = K_VALUE - 1; k > 0; k--)
		{
			lruKHistory[historyIdx].accessTimes[k] = lruKHistory[historyIdx].accessTimes[k - 1];
		}
		lruKHistory[historyIdx].accessTimes[0] = lruKTime;
		
		if(lruKHistory[historyIdx].accessCount < K_VALUE)
			lruKHistory[historyIdx].accessCount++;
	}
	
	lruKTime++;
}

// ***** BUFFER POOL FUNCTIONS ***** //

// initializes a buffer pool with numPages page frames
extern RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		  const int numPages, ReplacementStrategy strategy, 
		  void *stratData)
{
	bm->pageFile = (char *)pageFileName;
	bm->numPages = numPages;
	bm->strategy = strategy;

	// Reserver memory space = number of pages x space required for one page
	PageFrame *page = malloc(sizeof(PageFrame) * numPages);
	
	// Buffersize is the total number of pages in memory or the buffer pool.
	bufferSize = numPages;	
	int i;

	// Intilalizing all pages in buffer pool. The values of fields (variables) in the page is either NULL or 0
	for(i = 0; i < bufferSize; i++)
	{
		page[i].data = NULL;
		page[i].pageNum = -1;
		page[i].dirtyBit = 0;
		page[i].fixCount = 0;
		page[i].hitNum = 0;	
		page[i].refNum = 0;
	}

	bm->mgmtData = page;
	writeCount = clockPointer = lfuPointer = 0;
	
	// Initialize LRU-K history if using LRU-K strategy
	if(strategy == RS_LRU_K)
	{
		lruKHistory = (LRU_K_History *) malloc(sizeof(LRU_K_History) * numPages);
		for(i = 0; i < numPages; i++)
		{
			lruKHistory[i].pageNum = -1;
			lruKHistory[i].accessCount = 0;
			for(int j = 0; j < K_VALUE; j++)
			{
				lruKHistory[i].accessTimes[j] = 0;
			}
		}
		lruKTime = 0;
	}
	
	return RC_OK;
		
}

// Shutdown i.e. close the buffer pool, thereby removing all the pages from the memory and freeing up all resources and releasing some memory space.
extern RC shutdownBufferPool(BM_BufferPool *const bm)
{
	PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
	// Write all dirty pages (modified pages) back to disk
	forceFlushPool(bm);

	int i;	
	for(i = 0; i < bufferSize; i++)
	{
		// If fixCount != 0, it means that the contents of the page was modified by some client and has not been written back to disk.
		if(pageFrame[i].fixCount != 0)
		{
			return RC_WRITE_FAILED;
		}
	}

	// Free all page data buffers to prevent memory leaks
	for(i = 0; i < bufferSize; i++)
	{
		if(pageFrame[i].data != NULL)
		{
			free(pageFrame[i].data);
			pageFrame[i].data = NULL;
		}
	}

	// Free LRU-K history if it was allocated
	if(lruKHistory != NULL)
	{
		free(lruKHistory);
		lruKHistory = NULL;
	}

	// Releasing space occupied by the page
	free(pageFrame);
	bm->mgmtData = NULL;
	return RC_OK;
}

// This function writes all the dirty pages (having fixCount = 0) to disk
extern RC forceFlushPool(BM_BufferPool *const bm)
{
	PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
	
	int i;
	// Store all dirty pages (modified pages) in memory to page file on disk	
	for(i = 0; i < bufferSize; i++)
	{
		if(pageFrame[i].fixCount == 0 && pageFrame[i].dirtyBit == 1)
		{
			SM_FileHandle fh;
			// Opening page file available on disk
			openPageFile(bm->pageFile, &fh);
			// Writing block of data to the page file on disk
			writeBlock(pageFrame[i].pageNum, &fh, pageFrame[i].data);
			// Mark the page not dirty.
			pageFrame[i].dirtyBit = 0;
			// Increase the writeCount which records the number of writes done by the buffer manager.
			writeCount++;
		}
	}	
	return RC_OK;
}


// ***** PAGE MANAGEMENT FUNCTIONS ***** //

// This function marks the page as dirty indicating that the data of the page has been modified by the client
extern RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
	
	int i;
	// Iterating through all the pages in the buffer pool
	for(i = 0; i < bufferSize; i++)
	{
		// If the current page is the page to be marked dirty, then set dirtyBit = 1 (page has been modified) for that page
		if(pageFrame[i].pageNum == page->pageNum)
		{
			pageFrame[i].dirtyBit = 1;
			return RC_OK;		
		}			
	}		
	return RC_READ_NON_EXISTING_PAGE;
}

// This function unpins a page from the memory i.e. removes a page from the memory
extern RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{	
	PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
	
	int i;
	// Iterating through all the pages in the buffer pool
	for(i = 0; i < bufferSize; i++)
	{
		// If the current page is the page to be unpinned, then decrease fixCount (which means client has completed work on that page) and exit loop
		if(pageFrame[i].pageNum == page->pageNum)
		{
			pageFrame[i].fixCount--;
			break;		
		}		
	}
	return RC_OK;
}

// This function writes the contents of the modified pages back to the page file on disk
extern RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
	
	int i;
	// Iterating through all the pages in the buffer pool
	for(i = 0; i < bufferSize; i++)
	{
		// If the current page = page to be written to disk, then right the page to the disk using the storage manager functions
		if(pageFrame[i].pageNum == page->pageNum)
		{		
			SM_FileHandle fh;
			openPageFile(bm->pageFile, &fh);
			writeBlock(pageFrame[i].pageNum, &fh, pageFrame[i].data);
		
			// Mark page as undirty because the modified page has been written to disk
			pageFrame[i].dirtyBit = 0;
			
			// Increase the writeCount which records the number of writes done by the buffer manager.
			writeCount++;
		}
	}	
	return RC_OK;
}

// This function pins a page with page number pageNum i.e. adds the page with page number pageNum to the buffer pool.
// If the buffer pool is full, then it uses appropriate page replacement strategy to replace a page in memory with the new page being pinned. 
extern RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
	    const PageNumber pageNum)
{
	PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
	
	// Checking if buffer pool is empty and this is the first page to be pinned
	if(pageFrame[0].pageNum == -1)
	{
		// Reading page from disk and initializing page frame's content in the buffer pool
		SM_FileHandle fh;
		openPageFile(bm->pageFile, &fh);
		pageFrame[0].data = (SM_PageHandle) malloc(PAGE_SIZE);
		ensureCapacity(pageNum + 1, &fh);
		readBlock(pageNum, &fh, pageFrame[0].data);
		pageFrame[0].pageNum = pageNum;
		pageFrame[0].fixCount++;
		rearIndex = hit = 0;
		pageFrame[0].hitNum = hit;	
		pageFrame[0].refNum = 0;
		page->pageNum = pageNum;
		page->data = pageFrame[0].data;
		
		// Initialize LRU-K history for first page if using LRU-K
		if(bm->strategy == RS_LRU_K && lruKHistory != NULL)
		{
			lruKHistory[0].pageNum = pageNum;
			lruKHistory[0].accessTimes[0] = lruKTime;
			lruKHistory[0].accessCount = 1;
			lruKTime++;
		}
		
		return RC_OK;		
	}
	else
	{	
		int i;
		bool isBufferFull = true;
		
		for(i = 0; i < bufferSize; i++)
		{
			if(pageFrame[i].pageNum != -1)
			{	
				// Checking if page is in memory
				if(pageFrame[i].pageNum == pageNum)
				{
					// Increasing fixCount i.e. now there is one more client accessing this page
					pageFrame[i].fixCount++;
					isBufferFull = false;
					hit++; // Incrementing hit (hit is used by LRU algorithm to determine the least recently used page)

					if(bm->strategy == RS_LRU)
						// LRU algorithm uses the value of hit to determine the least recently used page	
						pageFrame[i].hitNum = hit;
					else if(bm->strategy == RS_CLOCK)
						// hitNum = 1 to indicate that this was the last page frame examined (added to the buffer pool)
						pageFrame[i].hitNum = 1;
					else if(bm->strategy == RS_LFU)
						// Incrementing refNum to add one more to the count of number of times the page is used (referenced)
						pageFrame[i].refNum++;
					else if(bm->strategy == RS_LRU_K)
					{
						// Update LRU-K history for this page access
						lruKTime++;
						for(int j = 0; j < bufferSize; j++)
						{
							if(lruKHistory[j].pageNum == pageNum)
							{
								// Shift access times and add new access
								for(int k = K_VALUE - 1; k > 0; k--)
								{
									lruKHistory[j].accessTimes[k] = lruKHistory[j].accessTimes[k - 1];
								}
								lruKHistory[j].accessTimes[0] = lruKTime;
								if(lruKHistory[j].accessCount < K_VALUE)
									lruKHistory[j].accessCount++;
								break;
							}
						}
					}
					
					page->pageNum = pageNum;
					page->data = pageFrame[i].data;

					clockPointer++;
					break;
				}				
			} else {
				SM_FileHandle fh;
				openPageFile(bm->pageFile, &fh);
				pageFrame[i].data = (SM_PageHandle) malloc(PAGE_SIZE);
				ensureCapacity(pageNum + 1, &fh);
				readBlock(pageNum, &fh, pageFrame[i].data);
				pageFrame[i].pageNum = pageNum;
				pageFrame[i].fixCount = 1;
				pageFrame[i].refNum = 0;
				rearIndex++;	
				hit++; // Incrementing hit (hit is used by LRU algorithm to determine the least recently used page)

				if(bm->strategy == RS_LRU)
					// LRU algorithm uses the value of hit to determine the least recently used page
					pageFrame[i].hitNum = hit;				
				else if(bm->strategy == RS_CLOCK)
					// hitNum = 1 to indicate that this was the last page frame examined (added to the buffer pool)
					pageFrame[i].hitNum = 1;
				else if(bm->strategy == RS_LRU_K && lruKHistory != NULL)
				{
					// Initialize LRU-K history for this new page
					for(int j = 0; j < bufferSize; j++)
					{
						if(lruKHistory[j].pageNum == -1)
						{
							lruKHistory[j].pageNum = pageNum;
							lruKHistory[j].accessTimes[0] = lruKTime;
							lruKHistory[j].accessCount = 1;
							lruKTime++;
							break;
						}
					}
				}
						
				page->pageNum = pageNum;
				page->data = pageFrame[i].data;
				
				isBufferFull = false;
				break;
			}
		}
		
		// If isBufferFull = true, then it means that the buffer is full and we must replace an existing page using page replacement strategy
		if(isBufferFull == true)
		{
			// Create a new page to store data read from the file.
			PageFrame *newPage = (PageFrame *) malloc(sizeof(PageFrame));		
			
			// Reading page from disk and initializing page frame's content in the buffer pool
			SM_FileHandle fh;
			openPageFile(bm->pageFile, &fh);
			newPage->data = (SM_PageHandle) malloc(PAGE_SIZE);
			ensureCapacity(pageNum + 1, &fh);
			readBlock(pageNum, &fh, newPage->data);
			newPage->pageNum = pageNum;
			newPage->dirtyBit = 0;		
			newPage->fixCount = 1;
			newPage->refNum = 0;
			rearIndex++;
			hit++;

			if(bm->strategy == RS_LRU)
				// LRU algorithm uses the value of hit to determine the least recently used page
				newPage->hitNum = hit;				
			else if(bm->strategy == RS_CLOCK)
				// hitNum = 1 to indicate that this was the last page frame examined (added to the buffer pool)
				newPage->hitNum = 1;

			page->pageNum = pageNum;
			page->data = newPage->data;			

			// Call appropriate algorithm's function depending on the page replacement strategy selected (passed through parameters)
			switch(bm->strategy)
			{			
				case RS_FIFO: // Using FIFO algorithm
					FIFO(bm, newPage);
					break;
				
				case RS_LRU: // Using LRU algorithm
					LRU(bm, newPage);
					break;
				
				case RS_CLOCK: // Using CLOCK algorithm
					CLOCK(bm, newPage);
					break;
  				
				case RS_LFU: // Using LFU algorithm
					LFU(bm, newPage);
					break;
  				
				case RS_LRU_K: // Using LRU-K algorithm
					LRU_K(bm, newPage);
					break;
				
				default:
					printf("\nAlgorithm Not Implemented\n");
					break;
			}
						
		}		
		return RC_OK;
	}	
}


// ***** STATISTICS FUNCTIONS ***** //

// This function returns an array of page numbers.
extern PageNumber *getFrameContents (BM_BufferPool *const bm)
{
	PageNumber *frameContents = malloc(sizeof(PageNumber) * bufferSize);
	PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
	
	int i = 0;
	// Iterating through all the pages in the buffer pool and setting frameContents' value to pageNum of the page
	while(i < bufferSize) {
		frameContents[i] = (pageFrame[i].pageNum != -1) ? pageFrame[i].pageNum : NO_PAGE;
		i++;
	}
	return frameContents;
}

// This function returns an array of bools, each element represents the dirtyBit of the respective page.
extern bool *getDirtyFlags (BM_BufferPool *const bm)
{
	bool *dirtyFlags = malloc(sizeof(bool) * bufferSize);
	PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
	
	int i;
	// Iterating through all the pages in the buffer pool and setting dirtyFlags' value to TRUE if page is dirty else FALSE
	for(i = 0; i < bufferSize; i++)
	{
		dirtyFlags[i] = (pageFrame[i].dirtyBit == 1) ? true : false ;
	}	
	return dirtyFlags;
}

// This function returns an array of ints (of size numPages) where the ith element is the fix count of the page stored in the ith page frame.
extern int *getFixCounts (BM_BufferPool *const bm)
{
	int *fixCounts = malloc(sizeof(int) * bufferSize);
	PageFrame *pageFrame= (PageFrame *)bm->mgmtData;
	
	int i = 0;
	// Iterating through all the pages in the buffer pool and setting fixCounts' value to page's fixCount
	while(i < bufferSize)
	{
		fixCounts[i] = (pageFrame[i].fixCount != -1) ? pageFrame[i].fixCount : 0;
		i++;
	}	
	return fixCounts;
}

// This function returns the number of pages that have been read from disk since a buffer pool has been initialized.
extern int getNumReadIO (BM_BufferPool *const bm)
{
	// Adding one because with start rearIndex with 0.
	return (rearIndex + 1);
}

// This function returns the number of pages written to the page file since the buffer pool has been initialized.
extern int getNumWriteIO (BM_BufferPool *const bm)
{
	return writeCount;
}
