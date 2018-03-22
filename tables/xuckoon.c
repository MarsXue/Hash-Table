/* * * * * * * * *
 * Multi-key Extendible Cuckoo Hashing
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by Wenqing Xue <wenqingx@student.unimelb.edu.au>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "xuckoon.h"

// macro to calculate the rightmost n bits of a number x
#define rightmostnbits(n, x) (x) & ((1 << (n)) - 1)

typedef struct bucket {
	int id;
	int depth;
	int nkeys;
	int64 *keys;
} Bucket;

typedef struct inner_table {
	Bucket **buckets;
	int size;
	int depth;
	int bucketsize;
	int nkeys;
} InnerTable;

struct xuckoon_table {
	InnerTable *table1;
	InnerTable *table2;
};

/******************************* HELP FUNCTION *******************************/
static Bucket *new_bucket(int first_address, int depth, int bucketsize);
static void double_inner_n_table(InnerTable *table);
static void reinsert_n_key(InnerTable *table, int64 key, int t);
static void new_inner_n_table(InnerTable *table, int bucketsize);
static void initialise_xuckoon_table(XuckoonHashTable *table, int size, 
	int bucketsize);
bool xuckoon_rehash_1(XuckoonHashTable *table, int64 key, int64 record, 
	int st, int check);
bool xuckoon_rehash_2(XuckoonHashTable *table, int64 key, int64 record, 
	int st, int check);
void free_inner_n_table(InnerTable *table);
bool inner_n_table_loopup(InnerTable *table, int64 key, int address);
/****************************************************************************/

static Bucket *new_bucket(int first_address, int depth, int bucketsize) {
	Bucket *bucket = malloc(sizeof *bucket);
	assert(bucket);

	bucket->id = first_address;
	bucket->depth = depth;
	bucket->nkeys = 0;
	bucket->keys = malloc((sizeof *bucket->keys) * bucketsize);
	assert(bucket->keys);

	return bucket;
}

static void double_inner_n_table(InnerTable *table) {
	int size = table->size * 2;
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	table->buckets = realloc(table->buckets, (sizeof *table->buckets) * size);
	assert(table->buckets);
	int i;
	for (i=0; i<table->size; i++) {
		table->buckets[table->size+i] = table->buckets[i];
	}
	table->size = size;
	table->depth++;
}

static void reinsert_n_key(InnerTable *table, int64 key, int t) {
	int address;
	if (t==1) {
		address = rightmostnbits(table->depth, h1(key));
	} else {
		address = rightmostnbits(table->depth, h2(key));
	}
	int order = table->buckets[address]->nkeys;
	table->buckets[address]->keys[order] = key;
	table->buckets[address]->nkeys++;
}

static void split_bucket(InnerTable *table, int address, int t) {
	if (table->buckets[address]->depth == table->depth) {
		double_inner_n_table(table);
	}
	Bucket *bucket = table->buckets[address];
	int depth = bucket->depth;
	int first_address = bucket->id;

	int new_depth = depth + 1;
	bucket->depth = new_depth;

	int new_first_address = 1 << depth | first_address;
	Bucket *newbucket = new_bucket(new_first_address, new_depth, table->bucketsize);

	int bit_address = rightmostnbits(depth, first_address);
	int suffix = (1 << depth) | bit_address;

	int maxprefix = 1 << (table->depth - new_depth);

	int prefix;
	for (prefix=0; prefix<maxprefix; prefix++) {
		int a = (prefix << new_depth) | suffix;
		table->buckets[a] = newbucket;
	}
	int num = bucket->nkeys;
	int i;
	int64 key;
	bucket->nkeys = 0;

	for (i=0; i<num; i++) {
		key = bucket->keys[i];
		reinsert_n_key(table, key, t);
	}
}

static void new_inner_n_table(InnerTable *table, int bucketsize) {
	assert(table);

	table->size = 1;
	table->depth = 0;
	table->bucketsize = bucketsize;
	table->nkeys = 0;

	table->buckets = malloc(sizeof *table->buckets);
	assert(table->buckets);
	table->buckets[0] = new_bucket(0, 0, bucketsize);

}

static void initialise_xuckoon_table(XuckoonHashTable *table, int size, 
	int bucketsize) {
	assert(table);
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	table->table1 = malloc((sizeof *table->table1) * size);
	assert(table->table1);
	new_inner_n_table(table->table1, bucketsize);

	table->table2 = malloc((sizeof *table->table2) * size);
	assert(table->table2);
	new_inner_n_table(table->table2, bucketsize);
}

XuckoonHashTable *new_xuckoon_hash_table(int bucketsize) {
	XuckoonHashTable *table = malloc(sizeof *table);
	assert(table);

	initialise_xuckoon_table(table, 1, bucketsize);
	return table;
}

void free_inner_n_table(InnerTable *table) {
	assert(table);

	int i;
	for (i=table->size-1; i>=0; i--) {
		if (table->buckets[i]->id == i) {
			free(table->buckets[i]->keys);
			free(table->buckets[i]);
		}
	}
	free(table->buckets);
	free(table);
}

void free_xuckoon_hash_table(XuckoonHashTable *table) {
	assert(table);
	free_inner_n_table(table->table1);
	free_inner_n_table(table->table2);
	free(table);
}

bool xuckoon_rehash_1(XuckoonHashTable *table, int64 key, int64 record, 
	int st, int check) {
	int hash = h1(key);
	int address = rightmostnbits(table->table1->depth, hash);

	if (key == record && st == 1 && check > 0) {
		split_bucket(table->table1, address, 1);
		address = rightmostnbits(table->table1->depth, hash);
		check = 0;
	}
	check++;

	int nkeys = table->table1->buckets[address]->nkeys;
	if (nkeys < table->table1->bucketsize) {
		table->table1->buckets[address]->keys[nkeys] = key;
		table->table1->buckets[address]->nkeys++;
		table->table1->nkeys++;
		return true;
	} else {
		// int rdm = rand() % (table->table1->bucketsize);
		// using random number occurs segmentation fault
		int old_key = table->table1->buckets[address]->keys[0];
		table->table1->buckets[address]->keys[0] = key;
		return xuckoon_rehash_2(table, old_key, record, st, check);
	}
}

bool xuckoon_rehash_2(XuckoonHashTable *table, int64 key, int64 record, 
	int st, int check) {
	int hash = h2(key);
	int address = rightmostnbits(table->table2->depth, hash);

	if (key == record && st == 2 && check > 0) {
		split_bucket(table->table2, address, 2);
		address = rightmostnbits(table->table2->depth, hash);
		check = 0;
	}
	check++;

	int nkeys = table->table2->buckets[address]->nkeys;
	if (nkeys < table->table2->bucketsize) {
		table->table2->buckets[address]->keys[nkeys] = key;
		table->table2->buckets[address]->nkeys++;
		table->table2->nkeys++;
		return true;
	} else {
		// int rdm = rand() % (table->table2->bucketsize);
		// using random number occurs segmentation fault
		int old_key = table->table2->buckets[address]->keys[0];
		table->table2->buckets[address]->keys[0] = key;
		return xuckoon_rehash_1(table, old_key, record, st, check);
	}
}

bool xuckoon_hash_table_insert(XuckoonHashTable *table, int64 key) {
	assert(table);

	if (xuckoon_hash_table_lookup(table, key)) {
		return false;
	}
	if (table->table1->nkeys <= table->table2->nkeys) {
		return xuckoon_rehash_1(table, key, key, 1, 0);
	} else {
		return xuckoon_rehash_2(table, key, key, 2, 0);
	}
}

bool inner_n_table_loopup(InnerTable *table, int64 key, int address) {
	int i;

	bool found = false;
	if (table->buckets[address]->nkeys != 0) {
		for (i=0; i<table->buckets[address]->nkeys; i++) {
			if (key == table->buckets[address]->keys[i]) {
				found = true;
			}
		}
	}
	return found;
}

bool xuckoon_hash_table_lookup(XuckoonHashTable *table, int64 key) {
	assert(table);

	int ht1 = rightmostnbits(table->table1->depth, h1(key));
	int ht2 = rightmostnbits(table->table2->depth, h2(key));

	if (inner_n_table_loopup(table->table1, key, ht1)) {
		return true;
	}
	if (inner_n_table_loopup(table->table2, key, ht2)) {
		return true;
	}
	return false;
}

void xuckoon_hash_table_print(XuckoonHashTable *table) {
	assert(table);
	printf("--- table ---\n");

	InnerTable *innertables[2] = {table->table1, table->table2};
	int t;
	for (t=0; t<2; t++) {
		printf("table %d\n", t+1);
		printf("  table:               buckets:\n");
		printf("  address | bucketid   bucketid [key]\n");

		int i, j;
		for (i=0; i<innertables[t]->size; i++) {
			printf("%9d | %-9d ", i, innertables[t]->buckets[i]->id);

			if (innertables[t]->buckets[i]->id == i) {
				printf("%9d ", innertables[t]->buckets[i]->id);
				printf("[");
				for (j=0; j<innertables[t]->bucketsize; j++) {
					if (j<innertables[t]->buckets[i]->nkeys) {
						printf(" %llu", innertables[t]->buckets[i]->keys[j]);
					} else {
						printf(" -");
					}
				}
				printf(" ]");
			}
			printf("\n");
		}
	}
	printf("--- end table ---\n");
}

void xuckoon_hash_table_stats(XuckoonHashTable *table) {
	assert(table);

	printf("--- table stats ---\n");
	printf("Table 1: %d items\n", table->table1->nkeys);
	printf("Table 2: %d items\n", table->table2->nkeys);
	printf("--- end stats ---\n");
}