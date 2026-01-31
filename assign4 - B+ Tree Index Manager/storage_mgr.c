/**
 * @file storage_mgr.c
 * @brief Storage Manager Implementation for Database Systems
 * 
 * This file implements a simple storage manager capable of reading and writing
 * fixed-size pages (blocks) to/from disk files. The storage manager handles
 * files with a page size of 4096 bytes (PAGE_SIZE) and provides both absolute
 * and relative addressing for page operations.
 * 
 * @author Nithish, Sunil
 * @date 2025
 */

#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<math.h>
#include "storage_mgr.h"

// Global file pointer for storage manager operations
FILE *pageFile;

/**
 * @brief Initialize the storage manager
 * 
 * Initializes the global storage manager state. Currently, this function
 * simply initializes the global file pointer to NULL. This function should
 * be called before any other storage manager operations.
 * 
 * @return void
 */
extern void initStorageManager (void) {
	// Initialising file pointer i.e. storage manager.
	pageFile = NULL;
}


/**
 * @brief Create a new page file with initial empty page
 * 
 * Creates a new page file with the specified name. The initial file size 
 * will be exactly one page (PAGE_SIZE bytes) filled with zero bytes.
 * If the file already exists, it will be overwritten.
 * 
 * @param fileName Name of the file to create (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if fileName is NULL, 
 *         RC_WRITE_FAILED if file creation fails
 */
RC createPageFile(char *fileName) {
    // Validate input parameters
    if (fileName == NULL) {
        THROW(RC_FILE_NOT_FOUND, "File name is NULL");
    }

    // Open file in write+read mode ("w+") - creates new file or overwrites existing
    FILE *pageFile = fopen(fileName, "w+");
    
    // Check if file was successfully opened
    if (pageFile == NULL) {
        THROW(RC_WRITE_FAILED, "Failed to create file");
    } else {
        // Allocate buffer for empty page and initialize to zero
        SM_PageHandle emptyPage = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
        if (emptyPage == NULL) {
            fclose(pageFile);
            THROW(RC_WRITE_FAILED, "Memory allocation failed");
        }

        // Write empty page to file - should write exactly PAGE_SIZE bytes
        if (fwrite(emptyPage, sizeof(char), PAGE_SIZE, pageFile) < PAGE_SIZE) {
            fclose(pageFile);
            free(emptyPage);
            THROW(RC_WRITE_FAILED, "Failed to write initial page");
        }

        // Clean up: close file and free memory
        fclose(pageFile);
        free(emptyPage);

        return RC_OK;
    }
}


/**
 * @brief Open an existing page file and initialize file handle
 * 
 * Opens an existing page file and initializes the provided file handle
 * with file metadata. The file must exist and be readable.
 * 
 * @param fileName Name of the file to open (must not be NULL)
 * @param fHandle Pointer to file handle to initialize (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist or parameters are NULL
 */
extern RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
    // Validate input parameters
    if (fileName == NULL || fHandle == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    // Open file in read/update binary mode ("rb+")
    FILE *pageFile = fopen(fileName, "rb+");
    if (pageFile == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    // Move to end of file to determine file size
    if (fseek(pageFile, 0L, SEEK_END) != 0) {
        fclose(pageFile);
        return RC_FILE_NOT_FOUND;
    }

    // Get file size and validate
    long fileSize = ftell(pageFile);
    if (fileSize < 0) {
        fclose(pageFile);
        return RC_FILE_NOT_FOUND;
    }

    // Initialize file handle with metadata
    fHandle->fileName = fileName;                    // Store filename
    fHandle->curPagePos = 0;                         // Start at first page
    fHandle->totalNumPages = fileSize / PAGE_SIZE;   // Calculate total pages
    fHandle->mgmtInfo = pageFile;                   // Store FILE pointer for operations

    // On success, do not close pageFile here. Caller is now responsible via mgmtInfo.
    return RC_OK;
}

/**
 * @brief Close an open page file
 * 
 * Closes the file associated with the given file handle and cleans up
 * the handle's management information.
 * 
 * @param fHandle Pointer to file handle to close (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if handle is invalid
 */
extern RC closePageFile(SM_FileHandle *fHandle) {
    // Validate file handle and management info
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        return RC_FILE_NOT_FOUND;

    // Close the file and clear management info
    FILE *fp = (FILE *)fHandle->mgmtInfo;
    fclose(fp);
    fHandle->mgmtInfo = NULL;
    return RC_OK;
}

/**
 * @brief Delete a page file from disk
 * 
 * Permanently deletes the specified page file from the file system.
 * The file must not be open when this function is called.
 * 
 * @param fileName Name of the file to delete (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist or fileName is NULL
 */
extern RC destroyPageFile(char *fileName) {
    // Validate input parameter
    if (fileName == NULL)
        return RC_FILE_NOT_FOUND;

    // Remove file from filesystem
    if (remove(fileName) != 0)
        return RC_FILE_NOT_FOUND; // File doesn't exist or permission error
    return RC_OK;
}

/**
 * @brief Read a specific page from file into memory
 * 
 * Reads the page at the specified page number from the file into the
 * provided memory buffer. The page number is 0-based.
 * 
 * @param pageNum Page number to read (0-based, must be valid)
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer to store page data (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if handle is invalid,
 *         RC_READ_NON_EXISTING_PAGE if page number is out of bounds
 */
extern RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Validate file handle and management info
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        return RC_FILE_NOT_FOUND;
    
    // Validate page number is within bounds
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        return RC_READ_NON_EXISTING_PAGE;

    FILE *fp = (FILE *)fHandle->mgmtInfo;

    // Seek to the correct byte offset for the requested page
    if (fseek(fp, pageNum * PAGE_SIZE, SEEK_SET) != 0)
        return RC_READ_NON_EXISTING_PAGE;

    // Read exactly PAGE_SIZE bytes into memory buffer
    if (fread(memPage, sizeof(char), PAGE_SIZE, fp) != PAGE_SIZE)
        return RC_READ_NON_EXISTING_PAGE;

    // Update current page position in file handle
    fHandle->curPagePos = pageNum;
    return RC_OK;
}

/**
 * @brief Get the current page position in file
 * 
 * Returns the current page position (0-based) in the file handle.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @return Current page position (0-based), or -1 if handle is invalid
 */
extern int getBlockPos(SM_FileHandle *fHandle) {
    if (fHandle == NULL)
        return -1;
    return fHandle->curPagePos;
}

/**
 * @brief Read the first page from file
 * 
 * Convenience function to read the first page (page 0) from the file.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer to store page data (must not be NULL)
 * @return RC_OK on success, error code from readBlock on failure
 */
extern RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(0, fHandle, memPage);
}

/**
 * @brief Read the previous page from file
 * 
 * Reads the page immediately before the current page position.
 * Updates the current page position to the previous page.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer to store page data (must not be NULL)
 * @return RC_OK on success, error code from readBlock on failure
 */
extern RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    int prevPage = fHandle->curPagePos - 1;
    return readBlock(prevPage, fHandle, memPage);
}

/**
 * @brief Read the current page from file
 * 
 * Reads the page at the current page position in the file handle.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer to store page data (must not be NULL)
 * @return RC_OK on success, error code from readBlock on failure
 */
extern RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(fHandle->curPagePos, fHandle, memPage);
}

/**
 * @brief Read the next page from file
 * 
 * Reads the page immediately after the current page position.
 * Updates the current page position to the next page.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer to store page data (must not be NULL)
 * @return RC_OK on success, error code from readBlock on failure
 */
extern RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    int nextPage = fHandle->curPagePos + 1;
    return readBlock(nextPage, fHandle, memPage);
}

/**
 * @brief Read the last page from file
 * 
 * Reads the last page in the file (highest page number).
 * Updates the current page position to the last page.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer to store page data (must not be NULL)
 * @return RC_OK on success, error code from readBlock on failure
 */
extern RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    int lastPage = fHandle->totalNumPages - 1;
    return readBlock(lastPage, fHandle, memPage);
}

/**
 * @brief Write a page to a specific position in file
 * 
 * Writes the provided page data to the specified page number in the file.
 * The page number is 0-based and must be within the file's current bounds.
 * 
 * @param pageNum Page number to write to (0-based, must be valid)
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer containing page data (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if handle is invalid,
 *         RC_WRITE_FAILED if page number is out of bounds or write fails
 */
extern RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Validate file handle and management info
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        return RC_FILE_NOT_FOUND;
    
    // Validate page number is within bounds
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        return RC_WRITE_FAILED;

    FILE *fp = (FILE *)fHandle->mgmtInfo;
    
    // Seek to the correct byte offset for the requested page
    if (fseek(fp, pageNum * PAGE_SIZE, SEEK_SET) != 0)
        return RC_WRITE_FAILED;
    
    // Write exactly PAGE_SIZE bytes from memory buffer
    if (fwrite(memPage, sizeof(char), PAGE_SIZE, fp) != PAGE_SIZE)
        return RC_WRITE_FAILED;

    // Update current page position in file handle
    fHandle->curPagePos = pageNum;
    return RC_OK;
}

/**
 * @brief Write data to the current page
 * 
 * Convenience function to write page data to the current page position
 * in the file handle.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @param memPage Pointer to memory buffer containing page data (must not be NULL)
 * @return RC_OK on success, error code from writeBlock on failure
 */
extern RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    int currentPage = fHandle->curPagePos;
    return writeBlock(currentPage, fHandle, memPage);
}



/**
 * @brief Append an empty page to the end of the file
 * 
 * Adds a new empty page (filled with zeros) to the end of the file.
 * Updates the file handle's total page count and positions the current
 * page to the newly added page.
 * 
 * @param fHandle Pointer to file handle (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if handle is invalid,
 *         RC_WRITE_FAILED if write operation fails
 */
extern RC appendEmptyBlock(SM_FileHandle *fHandle) {
    // Validate file handle and management info
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        return RC_FILE_NOT_FOUND;

    FILE *fp = (FILE *)fHandle->mgmtInfo;
    
    // Seek to end of file to append new page
    if (fseek(fp, 0, SEEK_END) != 0)
        return RC_WRITE_FAILED;

    // Allocate memory for empty page (initialized to zeros)
    char *emptyPage = (char *)calloc(PAGE_SIZE, sizeof(char));
    if (emptyPage == NULL)
        return RC_WRITE_FAILED;

    // Write empty page to end of file
    if (fwrite(emptyPage, sizeof(char), PAGE_SIZE, fp) != PAGE_SIZE) {
        free(emptyPage);
        return RC_WRITE_FAILED;
    }
    free(emptyPage);

    // Update file handle metadata
    fHandle->totalNumPages++;
    fHandle->curPagePos = fHandle->totalNumPages - 1; // Point to new page
    return RC_OK;
}

/**
 * @brief Ensure file has at least the specified number of pages
 * 
 * Extends the file to have at least the specified number of pages by
 * appending empty pages as needed. If the file already has enough pages,
 * no changes are made.
 * 
 * @param numberOfPages Minimum number of pages the file should have
 * @param fHandle Pointer to file handle (must not be NULL)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if handle is invalid,
 *         error code from appendEmptyBlock on failure
 */
extern RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {
    // Validate file handle and management info
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
        return RC_FILE_NOT_FOUND;

    // Append empty pages until we reach the required capacity
    while (fHandle->totalNumPages < numberOfPages) {
        RC status = appendEmptyBlock(fHandle);
        if (status != RC_OK)
            return status;
    }
    return RC_OK;
}
