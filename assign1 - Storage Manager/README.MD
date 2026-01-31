
# Storage Manager Implementation


### Authors: Nithish Kumar Thathaiahkalva, Sunil Kuruba
<!-- ### UIN :  -->
### Email: nthat@uic.edu, skuru@uic.edu

##  Description

This implementation provides a simple storage manager capable of reading and writing fixed-size pages (blocks) to/from disk files. The storage manager handles files with a page size of 4096 bytes (PAGE_SIZE) and provides both absolute and relative addressing for page operations.

## Solution Design

### Core Data Structures
- **SM_FileHandle**: Stores file metadata including filename, total pages, current position, and file pointer
- **SM_PageHandle**: Pointer to memory buffer for page data (4096 bytes)

### Key Design Decisions

1. **File Format**: Files are treated as simple binary files with no special header format. The total number of pages is calculated dynamically based on file size.

2. **Memory Management**: 
   - File handles store dynamically allocated filename strings
   - File pointers are stored in `mgmtInfo` field
   - Proper cleanup in `closePageFile()` to prevent memory leaks

3. **Error Handling**: 
   - Comprehensive error checking for all function parameters
   - Proper use of existing error codes from `dberror.h`
   - Helper functions for validation

4. **Position Management**: 
   - `curPagePos` tracks current page position for relative operations
   - Updated automatically during read/write operations
   - Bounds checking to prevent out-of-range access

## Code Structure

### File Operations
- `createPageFile()`: Creates new file with one page of zeros
- `openPageFile()`: Opens existing file and initializes handle
- `closePageFile()`: Closes file and frees resources
- `destroyPageFile()`: Deletes file from disk

### Read Operations
- `readBlock()`: Read specific page by number
- `readFirstBlock()`, `readLastBlock()`: Read first/last pages
- `readCurrentBlock()`, `readNextBlock()`, `readPreviousBlock()`: Relative reads
- `getBlockPos()`: Get current page position

### Write Operations
- `writeBlock()`: Write to specific page
- `writeCurrentBlock()`: Write to current page
- `appendEmptyBlock()`: Add new empty page
- `ensureCapacity()`: Ensure file has minimum number of pages

### Helper Functions
- `isValidFileHandle()`: Validates file handle integrity
- `getFileSize()`: Safely gets file size without changing position

## Implementation Highlights

1. **Robust Error Handling**: All functions check for NULL pointers and invalid parameters
2. **Memory Safety**: Proper allocation/deallocation of resources
3. **Position Tracking**: Accurate maintenance of current page position
4. **File I/O Safety**: Proper use of fseek/fread/fwrite with error checking
5. **Code Reuse**: Helper functions reduce code duplication

## Build Instructions

```bash
make clean    # Clean build artifacts
make          # Build the test executable
make test     # Build and run tests
```

## Testing

The implementation uses the professor-provided test cases in `test_assign1_1.c` that verify:
- File creation, opening, and closing
- Page reading and writing operations
- Error handling for invalid operations
- Memory management correctness

## Error Codes Used

- `RC_OK`: Successful operation
- `RC_FILE_NOT_FOUND`: File doesn't exist or cannot be accessed
- `RC_FILE_HANDLE_NOT_INIT`: Invalid or uninitialized file handle
- `RC_WRITE_FAILED`: Write operation failed
- `RC_READ_NON_EXISTING_PAGE`: Attempted to read non-existent page
