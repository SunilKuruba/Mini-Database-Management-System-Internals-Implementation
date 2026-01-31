# Mini Database Management System Internals Implementation

This project implements foundational **database management system (DBMS) internals** in C, including paging, buffering, record storage, and indexing. It was completed as part of **CS 581 – Advanced Database Management Systems** and demonstrates core concepts used in real-world database engines.

---

## Storage Manager Implementation

This component implements a lightweight **Storage Manager** that provides low-level disk I/O for a page-based database system by reading and writing **fixed-size 4096-byte pages** to binary files. The design is centered around `SM_FileHandle`, which tracks file metadata such as filename, total number of pages, current page position, and an internal file pointer stored in `mgmtInfo`, and `SM_PageHandle`, which represents an in-memory buffer for a single page. Page files are treated as headerless binary files, with the total number of pages computed dynamically from the file size. The implementation supports both **absolute addressing** (`readBlock`, `writeBlock`) and **relative navigation** (`readNextBlock`, `readPreviousBlock`, etc.) using `curPagePos`. Strong emphasis is placed on **robust error handling and validation** using predefined error codes from `dberror.h`, strict bounds checking for non-existing pages, and safe use of `fseek`, `fread`, and `fwrite`. Memory safety is ensured through proper allocation and cleanup of resources, with correctness verified using the instructor-provided test suite.

---

## Buffer Manager

This component implements a full **Buffer Manager** that serves as an in-memory cache layer between the database and disk, significantly reducing disk I/O by retaining frequently accessed pages in memory. The buffer pool consists of an array of `PageFrame` structures, each tracking the page number, pin count (`fixCount`), dirty status, page data, and access metadata. The manager supports core operations such as **pinning and unpinning pages**, **marking pages dirty**, and **forcing page flushes**, while integrating directly with the Storage Manager for disk reads, writes, and file capacity management. Two page replacement strategies are implemented: **FIFO**, using a circular victim pointer, and **LRU**, using a monotonically increasing logical timestamp to track recent access. Only unpinned frames are eligible for replacement, and dirty pages are flushed safely before eviction. The implementation also tracks I/O statistics (`readCount`, `writeCount`) and exposes runtime inspection APIs required by the test harness. All functionality is validated against the provided test suite, and memory correctness is verified using Valgrind within the course Docker environment.

---

## Record Manager Implementation

This component implements a **Record Manager** for fixed-schema tables, supporting **table creation, opening, closing, and deletion**, as well as **record insertion, update, deletion, retrieval, and scanning with optional predicates**. The design uses a **slotted-page architecture**, where each page contains multiple fixed-size record slots. Each slot begins with a **tombstone byte**, with `'+'` indicating an active record and `'-'` indicating a free or deleted slot, enabling efficient space reuse. All table metadata, including schema details, tuple count, and free-page hints, are stored in **page 0**, allowing the schema to be reconstructed when reopening a table. Record pages begin at page 1 and grow dynamically as needed. Insert operations leverage a “last known free page” optimization to minimize scanning overhead, while updates occur in place without changing record identifiers (RIDs). Scan operations maintain resumable state across calls and evaluate selection predicates using `evalExpr`, automatically skipping deleted records. The implementation carefully manages buffer pins and unpins, dirty page tracking, and dynamic memory allocation, and it passes all instructor-provided tests, including schema handling, persistence across reopen, record updates, deletions, and conditional scans.

---

## B+ Tree Index Manager

This component implements an **in-memory B+ Tree Index Manager** supporting index lifecycle operations along with **key insertion, lookup, deletion, and ordered scans** for **integer keys (DT_INT)**. The core data structure is a dynamically allocated `BTNode` that maintains sorted keys, parent pointers, child pointers for internal nodes, RID arrays for leaf nodes, and a `nextLeaf` pointer that links all leaf nodes into a sorted linked list for efficient sequential scans. Insertions locate the appropriate leaf through root-to-leaf traversal, insert keys in sorted order, and trigger **node splitting** when capacity exceeds the configured order. Split operations correctly propagate separator keys upward, create new roots when required, and maintain parent-child and leaf-link relationships. Search operations perform leaf localization followed by exact key matching, while scan operations iterate through the leaf chain to return entries in sorted order. Deletions remove keys from leaf nodes and update entry counts, without implementing underflow merging as per the simplified assignment requirements. All nodes are freed recursively during index deletion, and the implementation passes the full test suite with **no memory leaks**, as verified using Valgrind.

