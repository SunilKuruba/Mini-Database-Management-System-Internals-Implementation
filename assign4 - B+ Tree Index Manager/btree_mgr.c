#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "btree_mgr.h"
#include "dberror.h"
#include "tables.h"
#include <math.h>

typedef struct BTNode {
	bool isLeaf;
	int numKeys;
	struct BTNode *parent;
	struct BTNode *nextLeaf;
	int *keys;
	struct BTNode **children; // internal: numKeys+1
	RID *rids;                // leaf: numKeys
} BTNode;

typedef struct BTreeMgr {
	int order;
	int numNodes;
	int numEntries;
	BTNode *root;
} BTreeMgr;

typedef struct ScanState {
	BTNode *leaf;
	int index;
} ScanState;


static BTreeMgr *globalMgr = NULL;

// helpers
static void printNode(BTNode *node);

static BTNode *createNode(bool leaf) {
	BTNode *node = malloc(sizeof(BTNode));
	node->isLeaf = leaf;
	node->numKeys = 0;
	node->parent = NULL;
	node->nextLeaf = NULL;
	node->keys = calloc(globalMgr->order + 1, sizeof(int));
	node->children = calloc(globalMgr->order + 2, sizeof(BTNode *));
	node->rids = calloc(globalMgr->order + 1, sizeof(RID));
	globalMgr->numNodes++;
	return node;
}

static BTNode *findLeaf(BTNode *root, int key) {
	BTNode *cur = root;
	while (cur && !cur->isLeaf) {
		int i = 0;
		// Linear searching the node to find the child pointer
		while (i < cur->numKeys && key >= cur->keys[i]) i++;
		cur = cur->children[i];
	}
	return cur;
}

// Inserting into a leaf node
static void insertIntoLeaf(BTreeMgr *mgr, BTNode *leaf, int key, RID rid) {
	int i = leaf->numKeys - 1;
	while (i >= 0 && leaf->keys[i] > key) {
		leaf->keys[i + 1] = leaf->keys[i];
		leaf->rids[i + 1] = leaf->rids[i];
		i--;
	}
	leaf->keys[i + 1] = key;
	leaf->rids[i + 1] = rid;
	leaf->numKeys++;
	mgr->numEntries++;
}

static BTNode *splitAndPropagate(BTreeMgr *mgr, BTNode *node) {
	int keysCount = node->numKeys;
	int mid;
	if(keysCount%2 ==0) mid = keysCount/2;
    else mid = floor(keysCount/ 2)+1;

    bool leaf = node->isLeaf;
    BTNode *right = createNode(leaf);
    int promoteKey;

    if (leaf) {
        int move = node->numKeys - mid;
        memcpy(right->keys, node->keys + mid, move * sizeof(int));
        memcpy(right->rids, node->rids + mid, move * sizeof(RID));
        
		right->numKeys = move;
        node->numKeys = mid;

        right->nextLeaf = node->nextLeaf;
        node->nextLeaf = right;

        promoteKey = right->keys[0];
		printf("Promoting key %d from leaf node\n", promoteKey);
		printNode(node);
		printNode(right);
    } else {
        promoteKey = node->keys[mid];
        int move = node->numKeys - mid - 1;
        memcpy(right->keys, node->keys + mid + 1, move * sizeof(int));
        memcpy(right->children, node->children + mid + 1, (move + 1) * sizeof(BTNode *));
        
		right->numKeys = move;
        node->numKeys = mid;

        for (int i = 0; i <= right->numKeys; i++) right->children[i]->parent = right;
		// Cleaning up the old pointers to avoid dangling references
		for (int i = node->numKeys + 1; i < globalMgr->order + 2; i++) node->children[i] = NULL;
		for (int i = node->numKeys; i < globalMgr->order + 1; i++) node->keys[i] = 0;
    }
    right->parent = node->parent;

    // insert promoted key into parent if absent
    if (!node->parent) {
		printf("Creating new root...\n");
        BTNode *newRoot = createNode(false);
        newRoot->keys[0] = promoteKey;
        newRoot->children[0] = node;
        newRoot->children[1] = right;
        newRoot->numKeys = 1;
		newRoot->isLeaf = false;

        node->parent = right->parent = newRoot;
        mgr->root = newRoot;
        printNode(newRoot);
        return newRoot;
    }
	else{
    	// insert (promoteKey, right) into parent at the slot after `node`
    	BTNode *p = node->parent;
    	int idx = 0;
    	while (idx <= p->numKeys && p->children[idx] != node) idx++;

    	for (int i = p->numKeys; i > idx; i--) {
    	    p->keys[i] = p->keys[i - 1];
    	    p->children[i + 1] = p->children[i];
    	}
    	p->keys[idx] = promoteKey;
    	p->children[idx + 1] = right;
		p->numKeys++;
		printNode(p);
    	return (p->numKeys > globalMgr->order) ? splitAndPropagate(mgr, p) : node;
	}
}

static void freeNodes(BTNode *node) {
	if (!node) return;
	if (!node->isLeaf) {
		for (int i = 0; i <= node->numKeys; i++) {
			freeNodes(node->children[i]);
		}
	}
	free(node->keys);
	free(node->children);
	free(node->rids);
	free(node);
}


// Deleting from a leaf node
static RC deleteFromLeaf(BTreeMgr *mgr, BTNode *leaf, int key) {
	int idx = -1;
	for (int i = 0; i < leaf->numKeys; i++) {
		if (leaf->keys[i] == key) { idx = i; break; }
	}
	if (idx == -1) return RC_IM_KEY_NOT_FOUND;
	for (int i = idx + 1; i < leaf->numKeys; i++) {
		leaf->keys[i - 1] = leaf->keys[i];
		leaf->rids[i - 1] = leaf->rids[i];
	}
	leaf->numKeys--;
	mgr->numEntries--;
	return RC_OK;
}

RC initIndexManager (void *mgmtData) {
	return RC_OK;
}

RC shutdownIndexManager () {
	if(globalMgr){
		freeNodes(globalMgr->root);
		free(globalMgr);
		globalMgr = NULL;
	}
	return RC_OK;
}

// create, destroy, open, and close an btree index
RC createBtree (char *idxId, DataType keyType, int n) {
	if (globalMgr) return RC_OK;
	globalMgr = malloc(sizeof(BTreeMgr));
	globalMgr->order = n;
	globalMgr->numNodes = 0;
	globalMgr->numEntries = 0;
	globalMgr->root = createNode(true);
	return RC_OK;
}

RC openBtree (BTreeHandle **tree, char *idxId) {
	if (!globalMgr) return RC_FILE_NOT_FOUND;
	BTreeHandle *h = malloc(sizeof(BTreeHandle));
	h->idxId = idxId;
	h->keyType = DT_INT;
	h->mgmtData = globalMgr;
	*tree = h;
	return RC_OK;
}

RC closeBtree (BTreeHandle *tree) {
	tree->mgmtData = NULL;
	free(tree);
	return RC_OK;
}

RC deleteBtree (char *idxId) {
	if (globalMgr) {
		freeNodes(globalMgr->root);
		free(globalMgr);
		globalMgr = NULL;
	}
	return RC_OK;
}

// access information about a b-tree
RC getNumNodes (BTreeHandle *tree, int *result) {
	BTreeMgr *mgr = (BTreeMgr *) tree->mgmtData;
	*result = mgr->numNodes;
	return RC_OK;
}

RC getNumEntries (BTreeHandle *tree, int *result) {
	BTreeMgr *mgr = (BTreeMgr *)tree->mgmtData;
	*result = mgr->numEntries;
	return RC_OK;
}

RC getKeyType (BTreeHandle *tree, DataType *result) {
	*result = tree->keyType;
	return RC_OK;
}

RC findKey (BTreeHandle *tree, Value *key, RID *result) {
	if (tree == NULL) return RC_FILE_NOT_FOUND;
	if (key == NULL) return RC_IM_KEY_NOT_FOUND;

	BTreeMgr *mgr = (BTreeMgr *)tree->mgmtData;
	BTNode *leaf = findLeaf(mgr->root, key->v.intV);

	if (!leaf) return RC_IM_KEY_NOT_FOUND;
	for (int i = 0; i < leaf->numKeys; i++) {
		if (leaf->keys[i] == key->v.intV) {
			*result = leaf->rids[i];
			return RC_OK;
		}
	}

	return RC_IM_KEY_NOT_FOUND;
}

RC insertKey (BTreeHandle *tree, Value *key, RID rid) {
	if (tree == NULL) return RC_FILE_NOT_FOUND;
	if (key == NULL) return RC_IM_KEY_NOT_FOUND;

	BTreeMgr *mgr = (BTreeMgr *)tree->mgmtData;
	BTNode *leaf = findLeaf(mgr->root, key->v.intV);
	
	for (int i = 0; i < leaf->numKeys; i++) {
		if (leaf->keys[i] == key->v.intV) return RC_IM_KEY_ALREADY_EXISTS;
	}
	
	insertIntoLeaf(mgr, leaf, key->v.intV, rid);
	printf("Inserted key %d)\n", key->v.intV);
	printNode(leaf);
	printf("Leaf now has %d keys.\n", leaf->numKeys);

	if (leaf->numKeys > mgr->order) {
		printf("Node over flow, splitting...\n");
    	splitAndPropagate(mgr, leaf);
	}

	return RC_OK;
}

RC deleteKey (BTreeHandle *tree, Value *key) {
	if (tree == NULL) return RC_FILE_NOT_FOUND;
	if (key == NULL) return RC_IM_KEY_NOT_FOUND;

	BTreeMgr *mgr = (BTreeMgr *)tree->mgmtData;
	BTNode *leaf = findLeaf(mgr->root, key->v.intV);
	
	return deleteFromLeaf(mgr, leaf, key->v.intV);
}

RC *simplePrintTree (BTreeHandle *tree){
	BTreeMgr *mgr = (BTreeMgr *)tree->mgmtData;
	if (!mgr || !mgr->root) {
		printf("Empty tree.\n");
		return RC_OK;
	}
	// Simple BFS traversal
	BTNode **queue = malloc(sizeof(BTNode *) * mgr->numNodes);
	int front = 0, rear = 0;
	queue[rear++] = mgr->root;
	while (front < rear) {
		BTNode *node = queue[front++];
		printf("[");
		for (int i = 0; i < node->numKeys; i++) {
			printf("%d ,", node->keys[i]);
		}
		printf("] ");
		if (!node->isLeaf) {
			for (int i = 0; i <= node->numKeys; i++) {
				queue[rear++] = node->children[i];
			}
		}
	}
	printf("\n");
	free(queue);
	return RC_OK;
}

static void printNode(BTNode *node) {
    
    printf("%s node | keys: ", node->isLeaf ? "Leaf" : "Internal");
    if (!node->numKeys) printf("(empty)");
	else{
    for (int i = 0; i < node->numKeys; i++) {
        printf("%d ,", node->keys[i]);
    }}

    printf("\n");
}

RC openTreeScan (BTreeHandle *tree, BT_ScanHandle **handle){
	BTreeMgr *mgr = ((BTreeMgr *)tree->mgmtData);
	BTNode *node = mgr->root;
	if (!node) return RC_IM_NO_MORE_ENTRIES;

	ScanState *st = malloc(sizeof(ScanState));

	// This will find the leftmost leaf node
	while (node && !node->isLeaf) node = node->children[0];
	st->leaf = node;
	st->index = 0;

	BT_ScanHandle *scanHandle = malloc(sizeof(BT_ScanHandle));
	scanHandle->mgmtData = st;
	scanHandle->tree = tree;

	*handle = scanHandle;
	return RC_OK;
}

 RC nextEntry (BT_ScanHandle *handle, RID *result){
	ScanState *st = (ScanState *)handle->mgmtData;
	if (!st->leaf) return RC_IM_NO_MORE_ENTRIES;

	if (st->index >=st->leaf->numKeys) {
		st->leaf = st->leaf->nextLeaf;
		
		if (!st->leaf) return RC_IM_NO_MORE_ENTRIES;
		st->index = 0;
	}
	*result = st->leaf->rids[st->index];
	st->index++;
	return RC_OK;
 }

 RC closeTreeScan (BT_ScanHandle *handle){
	free(handle->mgmtData);
	free(handle);
	return RC_OK;
 }
