# Buffer Manager

### Authors: Nithish Kumar Thathaiahkalva, Sunil Kuruba

---

## Description

This project implements a Buffer Manager that provides an in-memory cache layer between the database and disk storage. The buffer manager maintains a fixed-size pool of page frames in memory, manages page replacement using FIFO and LRU strategies, handles dirty page writes, and tracks page access statistics. This implementation reduces disk I/O operations by keeping frequently accessed pages in memory and only writing modified pages back to disk when necessary.

---

## Solution Overview

We built a fully functional buffer pool manager that maintains page frames in memory, implemented efficient page replacement strategies, and provides caching for database pages. The implementation handles page pinning/unpinning, dirty flag management, and ensures data consistency through proper flushing mechanisms.

### What We Built

A complete Buffer Manager with:

* **Buffer Pool Management** – Initialize, shutdown, and manage fixed-size buffer pools
* **Page Operations** – Pin, unpin, mark dirty, and force flush individual pages
* **Replacement Strategies** – FIFO (First-In-First-Out) and LRU (Least Recently Used) algorithms
* **Statistics Tracking** – Monitor read/write counts, frame contents, dirty flags, and fix counts
* **Memory Management** – Dynamic allocation and proper deallocation of page frames
* **Disk Integration** – Integration with Storage Manager for disk I/O operations

---

## Implementation Details

### 1. Data Structures

#### `PageFrame` Structure
The Basic unit of the buffer pool:
- **`pageNum`** – Page number stored in this frame (NO_PAGE if empty)
- **`fixCount`** – Number of clients currently using this page (pin count)
- **`data`** – Pointer to actual page data (PAGE_SIZE bytes)
- **`dirtyFlag`** – Boolean indicating if page has been modified
- **`lastAccessedTime`** – Virtual timestamp for LRU replacement (incremented on each access)

#### `BufferManager` Structure
Global manager maintaining buffer pool metadata:
- **`pages`** – Array of PageFrame structures (size: numPages)
- **`readCount`** – Total number of pages read from disk
- **`writeCount`** – Total number of pages written to disk

#### `FIFO` Structure
Maintains state for FIFO replacement strategy:
- **`next`** – Index of the next frame to consider for replacement (circular-queue)

---

### 2. Buffer Pool Management Functions

#### `initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData)`
Initializes a new buffer pool:
- Creates `BufferManager` with specified number of page frames
- Allocates array of `PageFrame` structures
- Initializes all frames: pageNum=NO_PAGE, fixCount=0, dirtyFlag=false, data=NULL
- Sets replacement strategy (RS_FIFO or RS_LRU)
- Opens the page file using Storage Manager
- Initializes FIFO strategy state if needed
- Resets read/write counters to 0
- Returns RC_OK on success

#### `shutdownBufferPool(BM_BufferPool *const bm)`
Shuts down the buffer pool and frees all resources:
- Checks if any pages are still pinned (fixCount > 0)
- Returns RC_BUFFER_EXCEEDED if pinned pages exist
- Flushes all dirty pages to disk using `flushPage()`
- Frees page data for each frame
- Frees the pages array and BufferManager structure
- Frees FIFO strategy structure
- Closes the page file
- Returns RC_OK on success

---

### 3. Page Access Functions

#### `pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)`
Pins a page into the buffer pool for use:

**If page already in buffer:**
- Finds the page using `findPage()`
- Increments fixCount
- Updates lastAccessedTime for LRU tracking
- Sets page handle's pageNum and data pointer
- Returns RC_OK

**If page not in buffer:**
- Calls `findIndex()` to locate a victim frame
- Returns RC_BUFFER_EXCEEDED if no frame available
- Flushes victim frame if dirty using `flushPage()`
- Calls `loadPage()` to read new page from disk
- Initializes frame: pageNum, fixCount=1, dirtyFlag=false
- Updates lastAccessedTime
- Increments readCount
- Sets page handle's pageNum and data pointer
- Returns RC_OK

#### `unpinPage(BM_BufferPool *const bp, BM_PageHandle *const page)`
Unpins a page, indicating the client is done using it:
- Finds the page using `findPage()`
- Returns RC_IM_KEY_NOT_FOUND if page not in buffer
- Decrements fixCount if greater than 0
- Page becomes eligible for replacement when fixCount reaches 0
- Returns RC_OK

#### `markDirty(BM_BufferPool *const bp, BM_PageHandle *const page)`
Marks a page as modified (dirty):
- Finds the page using `findPage()`
- Returns RC_IM_KEY_NOT_FOUND if page not in buffer
- Sets dirtyFlag to true
- Dirty pages will be written back to disk before replacement
- Returns RC_OK

#### `forcePage(BM_BufferPool *const bp, BM_PageHandle *const page)`
Immediately writes a specific page to disk:
- Finds the page using `findPage()`
- Returns RC_IM_KEY_NOT_FOUND if page not in buffer
- Calls `flushPage()` to write page to disk
- Returns RC_OK

#### `forceFlushPool(BM_BufferPool *const bp)`
Writes all dirty pages in the buffer pool to disk:
- Iterates through all frames in the buffer
- Calls `flushPage()` for each frame
- Only dirty pages are actually written
- Returns RC_OK

---

### 4. Statistics Functions

#### `getFrameContents(BM_BufferPool *const bp)`
Returns an array of page numbers currently in the buffer pool:
- Allocates array of size numPages
- Fills array with pageNum from each frame
- Empty frames contain NO_PAGE (-1)
- Returns pointer to array (caller must free)

#### `getDirtyFlags(BM_BufferPool *const bp)`
Returns an array of dirty flags for all frames:
- Allocates boolean array of size numPages
- Fills array with dirtyFlag from each frame
- true indicates modified page, false indicates clean page
- Returns pointer to array (caller must free)

#### `getFixCounts(BM_BufferPool *const bp)`
Returns an array of fix counts (pin counts) for all frames:
- Allocates integer array of size numPages
- Fills array with fixCount from each frame
- 0 indicates unpinned page, >0 indicates pinned page
- Returns pointer to array (caller must free)

#### `getNumReadIO(BM_BufferPool *const bp)`
Returns the total number of pages read from disk since buffer pool initialization.

#### `getNumWriteIO(BM_BufferPool *const bp)`
Returns the total number of pages written to disk since buffer pool initialization.

---

### 5. Internal Helper Functions

#### `createBufferManager(int numPages)`
Allocates and initializes the BufferManager structure:
- Allocates BufferManager with malloc
- Initializes readCount and writeCount to 0
- Allocates pages array with numPages PageFrame structures
- Initializes each frame: pageNum=NO_PAGE, fixCount=0, dirtyFlag=false, data=NULL, lastAccessedTime=0
- Returns pointer to BufferManager

#### `getFrames(BM_BufferPool *const bp)`
Returns pointer to the pages array from the buffer pool's management data.

#### `findPage(BM_BufferPool *const bp, PageNumber pageNum)`
Searches for a specific page in the buffer pool:
- Iterates through all frames
- Compares frame's pageNum with target pageNum
- Returns pointer to PageFrame if found
- Returns NULL if not found

#### `loadPage(PageFrame *frame, BufferManager *bmgr, BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum)`
Loads a page from disk into a buffer frame:
- Allocates page data buffer (PAGE_SIZE) if needed
- Calls `ensureCapacity()` to extend file if necessary
- Calls `readBlock()` from Storage Manager to read page data
- Increments readCount
- Initializes frame metadata: pageNum, fixCount=1, dirtyFlag=false
- Updates lastAccessedTime with incremented accessCounter
- Sets page handle's pageNum and data pointer
- Returns RC_OK

#### `flushPage(PageFrame *frame, BM_BufferPool *const bp)`
Writes a dirty page to disk:
- Returns RC_OK if frame is empty (pageNum == NO_PAGE)
- Checks dirtyFlag
- If dirty, calls `writeBlock()` from Storage Manager
- Clears dirtyFlag after successful write
- Increments writeCount
- Returns RC_OK

#### `findIndex(BufferManager *bmgr, BM_BufferPool *bm)`
Finds a victim frame for page replacement:

**Phase 1: Look for empty frame**
- Iterates through all frames
- Returns index of first frame with pageNum == NO_PAGE

**Phase 2: Apply replacement strategy**

**FIFO Strategy:**
- Uses circular next pointer from FIFO structure
- Starts at last replacement position
- Searches circularly for unpinned frame (fixCount == 0)
- Updates next pointer for subsequent replacements
- Returns index of victim frame

**LRU Strategy:**
- Iterates through all frames
- Tracks frame with smallest lastAccessedTime among unpinned pages
- Returns index of least recently used unpinned frame

**Returns -1 if no frame available** (all frames pinned)

---

## Memory Leaks

We verified the project is leak-free by adding a `valgrind_test` target to the Makefile and running it inside the `iitdbgroup/cs581` Docker image. Valgrind reported no memory leaks for the test suite.

![alt text](<valgrind.jpeg>)

---

## Testing & Validation

### Test Coverage

The implementation passes the provided test suites:

**test_assign2_1.c** – Tests basic buffer manager operations:
- Buffer pool initialization and shutdown
- Page pinning and unpinning
- Dirty flag management
- Page replacement with FIFO and LRU strategies
- Statistics tracking (read/write counts)
- Force flush operations

### Build Instructions

```bash
make clean

make test1
./test_assign2_1

make test2
```

---

## Replacement Strategy Details

### FIFO (First-In First-Out)
- Maintains circular pointer to next frame to examine
- Searches sequentially for unpinned frame
- Simple and predictable behavior
- Does not consider page access patterns
- Good for sequential access workloads

### LRU (Least Recently Used)
- Uses virtual timestamps (accessCounter)
- Increments counter on every page access
- Selects unpinned frame with smallest timestamp
- Adapts to access patterns
- Better for workloads with temporal locality


---

## Conclusion

The buffer manager caches page file blocks in memory and supports FIFO and LRU replacement to keep frequently used pages resident. It tracks pins, dirty state, and I/O counts, flushing modified pages safely before eviction or shutdown.
