
# Record Manager Implementation

### Authors: Nithish Kumar Thathaiahkalva, Sunil Kuruba
### Email: nthat@uic.edu, skuru@uic.edu

---

## Description

This project implements a Record Manager that handles tables with fixed schema. The Record Manager provides functionality to insert, delete, update, and scan records in a table. It uses the Buffer Manager from Assignment 2 to handle page caching and the Storage Manager from Assignment 1 for disk I/O operations. The implementation supports variable-length records and includes a flexible scan mechanism with condition-based filtering.

---

## Solution Overview

We've designed this Record Manager with a slotted page architecture, where each page contains multiple fixed-size record slots. The implementation uses a tombstone mechanism to track deleted records and provides efficient scanning with expression evaluation support.

### What We Built
A comprehensive Record Manager with the following capabilities:
- **Table Management** - Create, open, close, and delete tables with schema definitions
- **Record Operations** - Insert, update, delete, and retrieve records
- **Scan Operations** - Sequential scan with optional condition filtering
- **Schema Management** - Support for multiple data types (INT, STRING, FLOAT, BOOL)
- **Memory Management** - Proper allocation and deallocation of all resources

---

## Implementation Details

### 1. Table and Record Manager Functions

These functions handle the lifecycle of the Record Manager and table management.

#### `initRecordManager()`
Initializes the Record Manager:
- Calls `initStorageManager()` to set up the underlying storage layer
- Prepares the system for table operations
- Returns RC_OK on success

#### `shutdownRecordManager()`
Shuts down the Record Manager:
- Frees any remaining allocated resources
- Cleans up the global record manager pointer
- Should be called before program termination

#### `createTable()`
Creates a new table with a given schema:
- **name**: The name of the table (also the page file name)
- **schema**: Defines the structure including attribute names, data types, and key information
- Creates a page file on disk
- Stores schema information in page 0 (metadata page):
  - Number of tuples (initially 0)
  - First free page (initially 1)
  - Number of attributes
  - Key size
  - Attribute names and data types
- Initializes buffer pool with LRU replacement strategy
- Writes the metadata page to disk

#### `openTable()`
Opens an existing table for operations:
- Loads the table's metadata from page 0
- Recreates the schema structure in memory
- Initializes the buffer pool for the table
- Sets up table management data structure
- Returns a handle to the opened table

#### `closeTable()`
Closes an open table:
- Calls `shutdownBufferPool()` to flush dirty pages and free buffer resources
- The schema is managed by the caller (test code handles freeing)
- Returns RC_OK on success

#### `deleteTable()`
Deletes a table from the system:
- Removes the page file from disk using `destroyPageFile()`
- Frees all associated resources
- The table cannot be used after deletion

#### `getNumTuples()`
Returns the number of records in the table:
- Accesses the management data structure
- Returns the tuplesCount field
- This count is maintained on every insert/delete operation

---

### 2. Record Functions

These functions handle individual record operations within a table.

#### `insertRecord()`
Inserts a new record into the table:
- Finds a free slot in the table using a linear search strategy
- Starts from the last known free page to minimize search time
- If current page is full, moves to the next page
- Creates new pages as needed when table grows
- Marks the slot with '+' tombstone to indicate an active record
- Copies record data into the slot
- Updates the record's RID (page and slot numbers)
- Increments the tuple count
- Marks the page as dirty to ensure persistence

#### `deleteRecord()`
Removes a record from the table:
- Locates the record using its RID (page number and slot number)
- Pins the page containing the record
- Changes the tombstone from '+' to '-' to mark it as deleted
- Marks the page as dirty
- Decrements the tuple count
- Unpins the page
- The slot becomes available for reuse in future insertions

#### `updateRecord()`
Updates an existing record in place:
- Locates the record using its RID
- Pins the page containing the record
- Keeps the '+' tombstone intact
- Copies the new data into the record slot
- Marks the page as dirty
- Unpins the page
- The RID remains unchanged

#### `getRecord()`
Retrieves a record from the table:
- Takes a RID to locate the record
- Pins the page containing the record
- Copies the record data into the provided record structure
- Sets the record's RID
- Unpins the page
- Returns RC_OK if successful, or an error code if the record doesn't exist

---

### 3. Scan Functions

These functions enable sequential scanning of table records with optional filtering.

#### `startScan()`
Initializes a scan operation on a table:
- **table**: The table to scan
- **scan**: Scan handle structure to store scan state
- **cond**: Boolean expression for filtering (NULL means scan all records)
- Allocates a scan manager structure
- Sets initial scan position (page 1, slot 0)
- Initializes scan count to 0
- Stores the condition for later evaluation
- Returns RC_OK to indicate scan is ready

#### `next()`
Returns the next record that matches the scan condition:
- Iterates through pages and slots sequentially
- Skips deleted records (tombstone != '+')
- For each valid record:
  - If no condition is specified, returns the record
  - If condition exists, evaluates it using `evalExpr()`
  - Returns the record if condition evaluates to TRUE
- Advances to next slot/page after returning a match
- Returns RC_RM_NO_MORE_TUPLES when all records have been scanned
- Maintains scan state between calls for resumable scanning

#### `closeScan()`
Terminates a scan operation:
- Frees the scan manager structure
- Cleans up any allocated resources
- The scan handle becomes invalid after this call
- Returns RC_OK on success

---

### 4. Schema Functions

These functions handle schema creation, access, and memory management.

#### `getRecordSize()`
Calculates the size of a record based on schema:
- Returns 1 byte for tombstone marker
- For each attribute, adds:
  - sizeof(int) for DT_INT
  - sizeof(float) for DT_FLOAT  
  - sizeof(bool) for DT_BOOL
  - Attribute length for DT_STRING
- Total size determines how many records fit per page

#### `createSchema()`
Creates a new schema structure:
- Takes number of attributes, attribute names, data types, type lengths, key size, and key attributes
- Allocates memory for the Schema structure
- Copies all schema information into the structure
- Returns pointer to the created schema

#### `freeSchema()`
Frees memory allocated for a schema:
- Includes NULL check for safety
- Only frees the schema structure itself
- Internal arrays (attrNames, dataTypes, typeLength, keyAttrs) are managed separately
- Returns RC_OK

#### `createRecord()`
Creates a new empty record:
- Allocates memory for Record structure
- Allocates memory for record data (size based on schema)
- Initializes RID to invalid values (-1, -1)
- Sets tombstone to '-' (empty/deleted marker)
- Returns RC_OK

#### `freeRecord()`
Frees memory allocated for a record:
- Frees the record data
- Frees the Record structure
- Returns RC_OK

#### `getAttr()`
Retrieves an attribute value from a record:
- Takes record, schema, and attribute number
- Calculates offset to the attribute in record data
- Skips tombstone byte (offset 1)
- Copies attribute data into provided Value structure
- Sets appropriate data type in Value
- Returns RC_OK

#### `setAttr()`
Sets an attribute value in a record:
- Takes record, schema, attribute number, and value
- Calculates offset to the attribute in record data
- Copies value data into the record at the correct offset
- Handles all data types (INT, FLOAT, BOOL, STRING)
- Returns RC_OK

---

## Technical Design

### Core Data Structures

#### **RecordManager**
Our internal management structure containing:
- **BM_PageHandle**: Handle for buffer manager page operations
- **BM_BufferPool**: Buffer pool for page caching
- **RID recordID**: Current record identifier (page and slot)
- **Expr *condition**: Scan condition for filtering
- **int tuplesCount**: Total number of records in table
- **int freePage**: Page number with known free slots
- **int scanCount**: Number of records scanned so far

#### **Schema**
Defines table structure:
- **int numAttr**: Number of attributes
- **char **attrNames**: Array of attribute names
- **DataType *dataTypes**: Array of data types (DT_INT, DT_STRING, etc.)
- **int *typeLength**: Array of type lengths (for strings)
- **int keySize**: Number of key attributes
- **int *keyAttrs**: Array of key attribute positions

#### **Record**
Represents a single record:
- **RID id**: Record identifier (page number and slot number)
- **char *data**: Pointer to record data (includes tombstone + attributes)

---

## Key Implementation Decisions

### Slotted Page Architecture
- Each page is divided into fixed-size slots
- Slot size = record size (calculated from schema)
- Number of slots per page = PAGE_SIZE / record size
- Simple and efficient for fixed-size records

### Tombstone Mechanism
- First byte of each record slot is a tombstone marker
- '+' indicates an active record
- '-' indicates a deleted or empty slot
- Allows quick identification of free slots during insertion
- Deleted slots can be reused for new records

### Schema Storage (Page 0)
Page 0 stores all metadata:
- Tuple count (4 bytes)
- Free page number (4 bytes)
- Number of attributes (4 bytes)
- Key size (4 bytes)
- For each attribute:
  - Attribute name (fixed 15 bytes)
  - Data type (4 bytes)
  - Type length (4 bytes)
- This allows the schema to be reconstructed when opening a table

### Scan Implementation
- Maintains scan state between next() calls
- Evaluates filter conditions using expression evaluator
- Skips deleted records automatically
- Returns RC_RM_NO_MORE_TUPLES when exhausted
- Handles NULL conditions (scan all records)
- Properly manages buffer pool pins/unpins

### Free Space Management
- Uses a simple "last known free page" strategy
- Avoids scanning from page 1 every time
- Linear search within a page for free slots
- Creates new pages when needed
- Simple but effective for most workloads

### Memory Management
- All allocated memory is properly freed
- Schema pointers managed carefully to avoid double-free
- closeTable() doesn't free schema (caller's responsibility)
- freeSchema() only frees the schema structure itself
- Buffer pool properly shut down on closeTable()

---

## Testing & Validation

### Test Suite: test_assign3_1.c

The implementation passes all provided test cases including:

1. **Test 1: Creating and Inserting Tuples** 
   - Creates a table with schema
   - Inserts 10 records with various data
   - Reads back all records and verifies content
   - Tests basic insert and retrieve operations

2. **Test 2: Attribute Manipulation**
   - Tests getAttr() and setAttr() functions
   - Verifies correct handling of all data types
   - Ensures attribute offsets are calculated correctly

3. **Test 3: Insert and Retrieve Operations**
   - Inserts multiple records
   - Retrieves each record by RID
   - Verifies all record data is correct
   - Tests table persistence (close and reopen)

4. **Test 4: Insert, Update, and Delete Operations**
   - Inserts records into table
   - Updates records in place
   - Deletes specific records
   - Verifies tuple count is maintained correctly
   - Tests tombstone mechanism

5. **Test 5: Scan Operations**
   - Creates a table with 10 records
   - Performs a scan with filter condition (attribute[2] == 1)
   - Verifies that exactly 2 matching records are found
   - Tests scan state management and condition evaluation

### Additional Test: test_expr.c
- Tests expression evaluation functionality
- Verifies boolean expressions (AND, OR, NOT operations)
- Tests value comparisons and operators
- Tests complex nested expressions
- Used by scan operations for filtering
- **All 3 test suites passing**: value serialization, boolean operators, and complex expressions

### Test Results Summary
All 5 main tests in test_assign3_1 passing
All 3 expression tests in test_expr passing  
No memory leaks detected

---

## What We Implemented

### Required Features
Table management (create, open, close, delete)  
Record operations (insert, update, delete, get)  
Schema management (create, free, attribute access)  
Scan operations with condition filtering  
Integration with Buffer Manager  
Proper memory management and cleanup  
Tombstone-based deletion  
RID-based record access  
Support for multiple data types (INT, STRING, FLOAT, BOOL)

---

## Build Instructions

The project uses a Makefile that compiles all necessary components from local sources.

### Build Commands
```bash
make clean         # Clean build artifacts
make               # Build all executables (test_assign3_1 and test_expr)
make test1         # Build and run main test suite
make test2         # Build and run expression tests
./test_assign3_1   # Run main tests directly
./test_expr        # Run expression tests directly
```

### Dependencies
The build system compiles all dependencies locally:
- Storage Manager (storage_mgr.c) - from Assignment 1
- Buffer Manager (buffer_mgr.c) - from Assignment 2  
- Record Manager (record_mgr.c) - current assignment
- Expression evaluator (expr.c) - for scan conditions
- Serialization (rm_serializer.c) - for schema/record serialization

---

## Project Structure

```
assign3/
├── README.md                  # Documentation
├── Makefile                   # Build configuration
├── record_mgr.h               # Record manager interface
├── record_mgr.c               # Record manager implementation
├── buffer_mgr.h               # Buffer manager header (from assign2)
├── buffer_mgr.c               # Buffer manager implementation (from assign2)
├── buffer_mgr_stat.h          # Buffer manager statistics header
├── buffer_mgr_stat.c          # Buffer manager statistics implementation
├── storage_mgr.h              # Storage manager header (from assign1)
├── storage_mgr.c              # Storage manager implementation (from assign1)
├── expr.h                     # Expression evaluation header
├── expr.c                     # Expression evaluation implementation
├── rm_serializer.c            # Schema serialization functions
├── dberror.h                  # Error code definitions
├── dberror.c                  # Error handling implementation
├── dt.h                       # Data type definitions
├── tables.h                   # Table structure definitions
├── test_assign3_1.c           # Main test suite
├── test_expr.c                # Expression evaluation tests
└── test_helper.h              # Test utilities
```

---

## Error Codes Used

We used and extended the error codes defined in `dberror.h`:

### Success Codes
- **RC_OK** - Successful operation

### File-related Errors
- **RC_FILE_NOT_FOUND** - Table/page file doesn't exist
- **RC_FILE_HANDLE_NOT_INIT** - Invalid file handle

### Record Manager Errors  
- **RC_RM_COMPARE_VALUE_OF_DIFFERENT_DATATYPE** - Type mismatch in comparison
- **RC_RM_EXPR_RESULT_IS_NOT_BOOLEAN** - Expression doesn't evaluate to boolean
- **RC_RM_BOOLEAN_EXPR_ARG_IS_NOT_BOOLEAN** - Boolean operation on non-boolean
- **RC_RM_NO_MORE_TUPLES** - Scan has exhausted all records
- **RC_RM_NO_PRINT_FOR_DATATYPE** - Cannot serialize data type
- **RC_RM_UNKOWN_DATATYPE** - Invalid data type

### Implementation-specific Returns
- **RC_SCAN_CONDITION_NOT_FOUND** - (Removed) Invalid scan condition
- **RC_READ_NON_EXISTING_PAGE** - Attempt to read invalid page
- **RC_WRITE_FAILED** - Write operation failed

---

## Conclusion

This Record Manager implementation provides a foundation for our database management system. It efficiently handles fixed-schema tables with support for multiple data types, provides flexible scanning with condition-based filtering, and integrates seamlessly with the Buffer Manager and Storage Manager from previous assignments. 
