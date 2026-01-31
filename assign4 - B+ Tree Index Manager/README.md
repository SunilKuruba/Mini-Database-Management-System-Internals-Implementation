# B+ Tree Index Manager

### Authors: Nithish Kumar Thathaiahkalva, Sunil Kuruba

---

## Description

This project implements a B+ Tree Index Manager data structure. The implementation uses dynamic memory allocation with C structs and pointers to manage the tree structure. All nodes are kept in Memory with parent/child relationships maintained through pointers, and leaf nodes are chained together to enable efficient sequential scans. This version does not persist data to disk, the tree exists only for the lifetime of the process.

---

## Solution Overview

We built a simplified yet functional B+ Tree that maintains all standard properties, keys are stored in sorted order, all data resides in leaf nodes, internal nodes serve as routing guides, and leaves form a linked list for range queries. The implementation focuses on core 
B+ Tree operations with correct splitting behavior according to assignment specifications.

### What We Built

A complete B+ Tree Index Manager with:

* **Index Management** – Create, open, close, and delete B+ Tree indexes (single global tree)
* **Key Operations** – Insert, find, and delete keys with associated RIDs
* **Scan Operations** – Sequential traversal of all entries in sorted order via leaf chain
* **Auto-Balancing** – Automatic node splitting during insertion to maintain tree balance
* **Memory Management** – Dynamic allocation and proper deallocation of all nodes
* **Integer Keys** – Support for DT_INT data type (as required by tests)

---

## Implementation Details

### 1. Data Structures

#### `BTNode` Structure
The fundamental building block of our B+ Tree:
- **`isLeaf`** – Boolean flag distinguishing leaf nodes from internal nodes
- **`numKeys`** – Current number of keys stored in the node
- **`parent`** – Pointer to parent node (NULL for root)
- **`nextLeaf`** – Pointer to next leaf in the chain (leaf nodes only)
- **`keys`** – Array of integer keys (size: order+1)
- **`children`** – Array of child pointers for internal nodes (size: order+2)
- **`rids`** – Array of RIDs for leaf nodes (size: order+1)

#### `BTreeMgr` Structure
Global manager maintaining tree metadata:
- **`order`** – Maximum number of keys per node (from createBtree parameter `n`)
- **`numNodes`** – Total nodes created (incremented during splits)
- **`numEntries`** – Total key-value pairs (updated on insert/delete)
- **`root`** – Pointer to the root node

#### `ScanState` Structure
Maintains scan position during tree traversal:
- **`leaf`** – Current leaf node being scanned
- **`index`** – Current position within the leaf's keys array

---

### 2. Index Manager Functions

#### `initIndexManager(void *mgmtData)`
Initializes the Index Manager and returns RC_OK. No actual initialization needed for this  implementation.

#### `shutdownIndexManager()`
Shuts down the Index Manager and returns RC_OK. Cleanup is handled by deleteBtree.

---

### 3. B+ Tree Management Functions

#### `createBtree(char *idxId, DataType keyType, int n)`
Creates a new B+ Tree index with order `n`:
- Allocates global `BTreeMgr` structure
- Sets order to `n` (not n+2 – the order parameter IS the maximum keys)
- Creates initial empty leaf node as root
- Initializes counters: numNodes=1, numEntries=0
- Returns RC_OK on success

#### `openBtree(BTreeHandle **tree, char *idxId)`
Opens the existing tree:
- Allocates `BTreeHandle` structure
- Associates handle with global `BTreeMgr`
- Sets keyType to DT_INT
- Returns RC_FILE_NOT_FOUND if no tree exists

#### `closeBtree(BTreeHandle *tree)`
Closes the tree handle:
- Clears the mgmtData pointer
- Does NOT free tree nodes (tree remains in memory)
- Returns RC_OK

#### `deleteBtree(char *idxId)`
Permanently deletes the tree:
- Recursively frees all nodes using `freeNodes()`
- Frees the `BTreeMgr` structure
- Sets global pointer to NULL
- Returns RC_OK

---

### 4. Data Access Functions

#### `getNumNodes(BTreeHandle *tree, int *result)`
Get total nodes using B-Tree Manager attribute mgr->numNodes:
- Returns the count via output parameter
- Returns RC_OK

#### `getNumEntries(BTreeHandle *tree, int *result)`
Get total entries using B-Tree Manager attribute mgr->numEntries:
- Returns the count via output parameter
- Returns RC_OK

#### `getKeyType(BTreeHandle *tree, DataType *result)`
Returns the key data type (always DT_INT in this implementation).

---

### 5. Key Operation Functions

#### `findKey(BTreeHandle *tree, Value *key, RID *result)`
Searches for a key in the tree:
- Validates tree and key parameters
- Calls `findLeaf()` to locate the appropriate leaf node
- Performs linear search within the leaf for exact match
- If found, copies RID to result and returns RC_OK
- If not found, returns RC_IM_KEY_NOT_FOUND

#### `insertKey(BTreeHandle *tree, Value *key, RID rid)`
Inserts a new key-RID pair:
- Validates tree and key parameters
- Calls `findLeaf()` to locate target leaf
- Checks for duplicates – returns RC_IM_KEY_ALREADY_EXISTS if key exists
- Calls `insertIntoLeaf()` to insert key and RID in sorted order
- If leaf overflows (numKeys > order), calls `splitAndPropagate()`
- Returns RC_OK on success

#### `deleteKey(BTreeHandle *tree, Value *key)`
Deletes a key from the tree:
- Validates tree and key parameters
- Calls `findLeaf()` to locate the leaf
- Calls `deleteFromLeaf()` to remove the key
- Returns RC_IM_KEY_NOT_FOUND if key doesn't exist
- Returns RC_OK on success
- **Note**: No underflow handling or node merging implemented

---

### 6. Tree Scan Functions

#### `openTreeScan(BTreeHandle *tree, BT_ScanHandle **handle)`
Initializes a sequential scan:
- Allocates `ScanState` structure
- Finds leftmost leaf by following first child pointers from root
- Sets index to 0 (start of first leaf)
- Allocates and returns `BT_ScanHandle`
- Returns RC_IM_NO_MORE_ENTRIES if tree is empty
- Returns RC_OK on success

#### `nextEntry(BT_ScanHandle *handle, RID *result)`
Returns the next entry in sorted order:
- Retrieves RID at current (leaf, index) position
- Advances index to next key
- If index reaches end of leaf, moves to nextLeaf and resets index to 0
- Returns RC_OK while entries remain
- Returns RC_IM_NO_MORE_ENTRIES when scan is complete

#### `closeTreeScan(BT_ScanHandle *handle)`
Terminates the scan:
- Frees `ScanState` structure
- Frees `BT_ScanHandle` structure
- Returns RC_OK

---

### 7. Validation

We verified the project is leak-free by adding a `valgrind_test` target to the Makefile and running it inside the `iitdbgroup/cs581` Docker image. Valgrind reported no memory leaks for the test suite.

---

### 8. Internal Helper Functions

#### `createNode(bool leaf)`
Allocates and initializes a new node:
- Allocates memory for `BTNode` structure
- Sets `isLeaf` flag based on parameter
- Allocates arrays: keys (order+1), children (order+2), rids (order+1)
- Initializes: numKeys=0, parent=NULL, nextLeaf=NULL
- Increments global numNodes counter
- Returns pointer to new node

#### `findLeaf(BTNode *root, int key)`
Traverses from root to appropriate leaf:
- Starts at root and walks down the tree
- At each internal node, performs linear search through keys
- Follows child pointer at position where key should reside
- Returns pointer to the leaf node that should contain the key

#### `insertIntoLeaf(BTreeMgr *mgr, BTNode *leaf, int key, RID rid)`
Inserts key-RID into leaf while maintaining sorted order:
- Finds insertion point using backward linear search
- Shifts larger keys and RIDs to the right
- Inserts new key and RID at correct position
- Increments leaf's numKeys counter
- Increments global numEntries counter

#### `splitAndPropagate(BTreeMgr *mgr, BTNode *node)`
Handles node overflow by splitting:

**For Leaf Nodes:**
- Calculates split point: `mid = (numKeys % 2 == 0) ? numKeys/2 : floor(numKeys/2)+1`
- Creates new right leaf
- Copies keys[mid..end] and rids[mid..end] to right leaf
- Updates numKeys for both leaves
- Links right leaf into chain: `right->nextLeaf = node->nextLeaf; node->nextLeaf = right`
- Promotes first key of right leaf to parent

**For Internal Nodes:**
- Calculates split point (same formula)
- Creates new right internal node
- Promotes key at position `mid` (this key moves up, not copied)
- Copies keys[mid+1..end] and children[mid+1..end] to right node
- Updates parent pointers for all moved children
- Updates numKeys for both nodes

**Parent Insertion:**
- If node has no parent (root split), creates new root with promoted key
- Otherwise, inserts promoted key and right child into parent at appropriate position
- Recursively splits parent if it also overflows
- Returns new root (may have changed due to root split)

#### `deleteFromLeaf(BTreeMgr *mgr, BTNode *leaf, int key)`
Removes a key from a leaf node:
- Performs linear search to find key index
- Returns RC_IM_KEY_NOT_FOUND if key not present
- Shifts remaining keys and RIDs left to fill gap
- Decrements leaf's numKeys counter
- Decrements global numEntries counter
- Returns RC_OK on success

#### `freeNodes(BTNode *node)`
Recursively frees all nodes in the tree:
- Post-order traversal: frees children before parent
- For internal nodes, recursively frees all child subtrees
- Frees arrays: keys, children, rids
- Frees node structure itself

#### `simplePrintTree(BTreeHandle *tree)` / `printNode(BTNode *node)`
Debug helpers for visualizing tree structure:
- `simplePrintTree`: Performs breadth-first traversal and prints all keys
- `printNode`: Prints node type and keys for debugging during insertion/splitting
- Used during development with printf statements to trace operations

---

## Memory Leaks

We verified the project is leak-free by adding a `valgrind_test` target to the Makefile and running it inside the `iitdbgroup/cs581` Docker image. Valgrind reported no memory leaks for the test suite.

<img width="957" height="290" alt="image" src="https://github.com/user-attachments/assets/e08d251f-3b0a-420f-845f-27ad9a52fee1" />

---

## Testing & Validation

### Test Coverage

The implementation passes the provided test suites:

**test_assign4_1.c** – Tests core B+ Tree operations:
- Index creation, opening, closing, deletion
- Key insertion with automatic splitting
- Key search and retrieval
- Key deletion
- Node and entry counting
- Tree scanning

### Build Instructions

```bash
make clean

make test1
./test_assign4_1

make test2
./test_expr

```

---

## Conclusion

This B+ Tree implementation provides a working in-memory index structure that demonstrates core B+ Tree algorithms including node splitting, leaf chaining, and sequential scanning. While simplified compared to production database systems, it correctly implements the fundamental operations and passes all provided tests.
