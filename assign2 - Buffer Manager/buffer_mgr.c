#include<stdio.h>
#include<stdlib.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <limits.h>
#include <math.h>
#include <time.h>

typedef struct PageFrame {
    PageNumber pageNum;
    int fixCount;
    char * data;
    bool dirtyFlag;
    long long lastAccessedTime; // We implemented LRU using virtual time-based approach
} PageFrame;

typedef struct BufferManager {
    PageFrame *pages;
    int readCount;
    int writeCount;
} BufferManager;

// Using it for LRU, this will increment with every pin
static long long accessCounter = 0;

typedef struct FIFO {
    int next;
} FIFO;

FIFO *fifoStrategy = NULL;

// Forward declarations of helper functions
static PageFrame *getFrames(BM_BufferPool *const bp);
static PageFrame *findPage(BM_BufferPool *const bp, PageNumber pageNum);
static int findIndex(BufferManager *bmgr, BM_BufferPool *bm);
static RC flushPage(PageFrame *frame, BM_BufferPool *const bp);


BufferManager* createBufferManager(int numPages) {
    BufferManager *bm = (BufferManager *) malloc (sizeof(BufferManager));
    bm->readCount = 0;
    bm->writeCount = 0;

    bm->pages = (PageFrame *) malloc (sizeof(PageFrame) * numPages);

    for (int i = 0; i < numPages; i++) {
        bm->pages[i].pageNum = NO_PAGE;
        bm->pages[i].fixCount = 0;
        bm->pages[i].dirtyFlag = false;
        bm->pages[i].data = NULL;
        bm->pages[i].lastAccessedTime = 0;
    }
    return bm;
}

// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,  
    const int numPages, ReplacementStrategy strategy, void *stratData) {
    BufferManager *bufferManager = createBufferManager(numPages);

    bm->pageFile = (char *)pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = bufferManager;

    fifoStrategy = (FIFO *) malloc (sizeof(FIFO));
    fifoStrategy->next = 0;
    
    return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm) {
    BufferManager *bmgr = (BufferManager *)bm->mgmtData;
    if (!bmgr) {
        return RC_OK;
    }

    PageFrame *frames = bmgr->pages;
    for (int i = 0; i < bm->numPages; i++) {
        // if (frames[i].fixCount > 0) return RC_BUFFER_EXCEEDED;
        if(frames[i].fixCount==0 && frames[i].dirtyFlag) flushPage(&frames[i], bm);
        if (frames[i].data) free(frames[i].data);
    }
    free(frames);
    free(bmgr);
    bm->mgmtData = NULL;
    if (fifoStrategy) { 
        free(fifoStrategy); 
        fifoStrategy = NULL; 
    }
    return RC_OK;
}

// Helper functions
static PageFrame *getFrames(BM_BufferPool *const bp) {
    BufferManager *bm = (BufferManager *)bp->mgmtData;
    return bm->pages;
}

static PageFrame *findPage(BM_BufferPool *const bp, PageNumber pageNum) {
    PageFrame *frames = getFrames(bp);

    for (int i = 0; i < bp->numPages; i++) {
        if (frames[i].pageNum == pageNum) {
            // printf("findPage match index %d for pageNum %d\n", i, pageNum);
            return &frames[i];
        }
    }
    return NULL;
}

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bp, BM_PageHandle *const page){
    PageFrame *frame = findPage(bp, page->pageNum);
    if (!frame) return RC_IM_KEY_NOT_FOUND;
    frame->dirtyFlag = true;
    return RC_OK;
}

RC forcePage (BM_BufferPool *const bp, BM_PageHandle *const page){
    PageFrame *frame = findPage(bp, page->pageNum);
    if(!frame) return RC_IM_KEY_NOT_FOUND;
    
    return flushPage(frame, bp);
}

RC forceFlushPool(BM_BufferPool *const bp){
    PageFrame *frames = getFrames(bp);

    for (int i = 0; i < bp->numPages; i++) {
        flushPage(&frames[i], bp);
    }
    return RC_OK;
}

RC loadPage(PageFrame *frame, BufferManager *bmgr, BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum) {
    if (frame->data == NULL) {
        frame->data = (SM_PageHandle) malloc(PAGE_SIZE);
    }

    SM_FileHandle fh;
    openPageFile(bm->pageFile, &fh);
    ensureCapacity(pageNum + 1, &fh);
    readBlock(pageNum, &fh, frame->data);
    closePageFile(&fh);
    bmgr->readCount++;

    frame->pageNum = pageNum;
    frame->fixCount = 1;
    frame->dirtyFlag = false;
    frame->lastAccessedTime = ++accessCounter;

    page->pageNum = pageNum;
    page->data = frame->data;
    return RC_OK;
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
		const PageNumber pageNum) {
    BufferManager *bmgr = (BufferManager *) bm->mgmtData;
    PageFrame *pageFrame = findPage(bm, pageNum);

    // if page is already in buffer pool, just pin it
    if (pageFrame) {
        pageFrame->fixCount++;
        pageFrame->lastAccessedTime = ++accessCounter;
        page->pageNum = pageNum;
        page->data = pageFrame->data;
        return RC_OK;
    }

    // otherwise, we need to find a victim page to replace
    int victimIndex = findIndex(bmgr, bm);

    if (victimIndex == -1) {
        return RC_BUFFER_EXCEEDED;
    }

    flushPage(&bmgr->pages[victimIndex],bm);
    loadPage(&bmgr->pages[victimIndex], bmgr, bm, page, pageNum);
    return RC_OK;
}

static int findIndex(BufferManager *bmgr, BM_BufferPool *bm) {
    // first look for empty frame
    for (int i = 0; i < bm->numPages; i++) {
        if (bmgr->pages[i].pageNum == NO_PAGE) return i;
    }

    // otherwise use strategy
    switch (bm->strategy) {
        case RS_FIFO: {
            int start = fifoStrategy->next;
            for (int k = 0; k < bm->numPages; k++) {
                int idx = (start + k) % bm->numPages;
                if (bmgr->pages[idx].fixCount == 0) {
                    fifoStrategy->next = (idx + 1) % bm->numPages;
                    return idx;
                }
            }
            break;
        }
        case RS_LRU: {
            int lruIndex = -1;
            long long leastTimestamp = LLONG_MAX; // Initializing with max value for starting iteration
            PageFrame *frames = getFrames(bm);
            for (int i = 0; i < bm->numPages; i++) {
                if (frames[i].fixCount == 0 && frames[i].lastAccessedTime < leastTimestamp) {
                    leastTimestamp = frames[i].lastAccessedTime;
                    lruIndex = i;
                }
            }
            return lruIndex;
        }
        default:
            break;
    }
    return -1;
}

RC unpinPage (BM_BufferPool *const bp, BM_PageHandle *const page){
    PageFrame *frame = findPage(bp, page->pageNum);
    if(!frame) return RC_IM_KEY_NOT_FOUND;

    if (frame->fixCount > 0) frame->fixCount--;
    return RC_OK;
}

static RC flushPage(PageFrame *frame, BM_BufferPool *const bp) {
    if (frame->pageNum == NO_PAGE) return RC_OK;
    if(frame->dirtyFlag){
        SM_FileHandle fh;
        openPageFile(bp->pageFile, &fh);
        ensureCapacity(frame->pageNum + 1, &fh);
        writeBlock(frame->pageNum, &fh, frame->data);
        closePageFile(&fh);

        frame->dirtyFlag = false;
        BufferManager *bm = (BufferManager *)bp->mgmtData;
        bm->writeCount++;
    }
    return RC_OK;
}

// Statistics Interface
PageNumber *getFrameContents (BM_BufferPool *const bp){
    PageFrame *frames = getFrames(bp);

    PageNumber *contents = (PageNumber *) malloc (sizeof(PageNumber) * bp->numPages);
    for (int i = 0; i < bp->numPages; i++) {
        contents[i] = frames[i].pageNum;
    }
    return contents;
}

bool *getDirtyFlags (BM_BufferPool *const bp){
    PageFrame *frames = getFrames(bp);

    bool *flags = (bool *) malloc (sizeof(bool) * bp->numPages);
    for (int i = 0; i < bp->numPages; i++) {
        flags[i] = frames[i].dirtyFlag;
    }
    return flags;
}

int *getFixCounts (BM_BufferPool *const bp){
    PageFrame *frames = getFrames(bp);

    int *fixCounts = (int *) malloc (sizeof(int) * bp->numPages);
    for (int i = 0; i < bp->numPages; i++) {
        fixCounts[i] = frames[i].fixCount;
    }
    return fixCounts;
}

int getNumReadIO (BM_BufferPool *const bp){
    BufferManager *bm = (BufferManager *)bp->mgmtData;
    return bm->readCount;
}

int getNumWriteIO (BM_BufferPool *const bp){
    BufferManager *bm = (BufferManager *)bp->mgmtData;
    return bm->writeCount;
}
