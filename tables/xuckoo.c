/* * * * * * * * *
* Dynamic hash table using a combination of extendible hashing and cuckoo
* hashing with a single keys per bucket, resolving collisions by switching keys 
* between two tables with two separate hash functions and growing the tables 
* incrementally in response to cycles
*
* created for COMP20007 Design of Algorithms - Assignment 2, 2017
* by Wenqing Xue <wenqingx@student.unimelb.edu.au>
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "xuckoo.h"

// macro to calculate the rightmost n bits of a number x
#define rightmostnbits(n, x) (x) & ((1 << (n)) - 1)

// a bucket stores a single key (full=true) or is empty (full=false)
// it also knows how many bits are shared between possible keys, and the first 
// table address that references it
typedef struct bucket {
	int id;		// a unique id for this bucket, equal to the first address
				// in the table which points to it
	int depth;	// how many hash value bits are being used by this bucket
	bool full;	// does this bucket contain a key
	int64 key;	// the key stored in this bucket
} Bucket;

// an inner table is an extendible hash table with an array of slots pointing 
// to buckets holding up to 1 key, along with some information about the number 
// of hash value bits to use for addressing
typedef struct inner_table {
	Bucket **buckets;	// array of pointers to buckets
	int size;			// how many entries in the table of pointers (2^depth)
	int depth;			// how many bits of the hash value to use (log2(size))
	int nkeys;			// how many keys are being stored in the table
} InnerTable;

// a xuckoo hash table is just two inner tables for storing inserted keys
struct xuckoo_table {
	InnerTable *table1;
	InnerTable *table2;
	// int time;
};

/******************************* HELP FUNCTION *******************************/
static Bucket *new_bucket(int first_address, int depth);
static void double_inner_table(InnerTable *table);
static void reinsert_key(InnerTable *table, int64 key, int t);
static void split_bucket(InnerTable *table, int address, int t);
static void new_inner_table(InnerTable *table);
static void initialise_xuckoo_table(XuckooHashTable *table, int size);
void free_x_inner_table(InnerTable *table);
bool inner_table_insert(InnerTable *table, int64 key, int t);
bool xuckoo_rehash_1(XuckooHashTable *table, int64 key, int64 record, int st, 
	int check);
bool xuckoo_rehash_2(XuckooHashTable *table, int64 key, int64 record, int st, 
	int check);
/****************************************************************************/

// the code was sourced from "xtndbl1.c"
static Bucket *new_bucket(int first_address, int depth) {
	Bucket *bucket = malloc(sizeof *bucket);
	assert(bucket);

	bucket->id = first_address;
	bucket->depth = depth;
	bucket->full = false;

	return bucket;
}

/****************************************************************************/

// the code was sourced from "xtndbl1.c"
static void double_inner_table(InnerTable *table) {
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

// the code was sourced from "xtndbl1.c"
static void reinsert_key(InnerTable *table, int64 key, int t) {
	int address;
	if (t==1) {
		address = rightmostnbits(table->depth, h1(key));
	} else {
		address = rightmostnbits(table->depth, h2(key));
	}
	table->buckets[address]->key = key;
	table->buckets[address]->full = true;
}

// the code was sourced from "xtndbl1.c"
static void split_bucket(InnerTable *table, int address, int t) {
	if (table->buckets[address]->depth == table->depth) {
		double_inner_table(table);
	}
	Bucket *bucket = table->buckets[address];
	int depth = bucket->depth;
	int first_address = bucket->id;

	int new_depth = depth + 1;
	bucket->depth = new_depth;

	int new_first_address = 1 << depth | first_address;
	Bucket *newbucket = new_bucket(new_first_address, new_depth);

	int bit_address = rightmostnbits(depth, first_address);
	int suffix = (1 << depth) | bit_address;

	int maxprefix = 1 << (table->depth - new_depth);

	int prefix;
	for (prefix=0; prefix < maxprefix; prefix++) {
		int a = (prefix << new_depth) | suffix;
		table->buckets[a] = newbucket;
	}
	int64 key = bucket->key;
	bucket->full = false;
	reinsert_key(table, key, t);
}

/****************************************************************************/

// the code was sourced from "xtndbl1.c"
static void new_inner_table(InnerTable *table) {
	assert(table);
	assert(table->size < MAX_TABLE_SIZE && "error: table has grown too large!");

	table->size = 1;
	table->buckets = malloc(sizeof *table->buckets);
	assert(table->buckets);
	table->buckets[0] = new_bucket(0, 0);
	table->depth = 0;
	table->nkeys = 0;
}

static void initialise_xuckoo_table(XuckooHashTable *table, int size) {
	assert(table);
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");
	// table->time = 0;

	table->table1 = malloc((sizeof *table->table1) * size);
	assert(table->table1);
	new_inner_table(table->table1);

	table->table2 = malloc((sizeof *table->table2) * size);
	assert(table->table2);
	new_inner_table(table->table2);
}

// initialise an extendible cuckoo hash table
XuckooHashTable *new_xuckoo_hash_table() {
	XuckooHashTable *table = malloc(sizeof *table);
	assert(table);

	initialise_xuckoo_table(table, 1);
	return table;
}

/****************************************************************************/

// the code was sourced from "xtndbl1.c"
void free_x_inner_table(InnerTable *table) {
	assert(table);
	
	int i;
	for (i=table->size-1; i>=0; i--) {
		if (table->buckets[i]->id == i) {
			free(table->buckets[i]);
		}
	}
	free(table->buckets);
	free(table);
}

// free all memory associated with 'table'
void free_xuckoo_hash_table(XuckooHashTable *table) {
	assert(table);
	free_x_inner_table(table->table1);
	free_x_inner_table(table->table2);
	free(table);
}

/****************************************************************************/

bool xuckoo_rehash_1(XuckooHashTable *table, int64 key, int64 record, int st, 
	int check) {
	int hash = h1(key);
	int address = rightmostnbits(table->table1->depth, hash);

	// st is start table
	// infinite loop occurs, split bucket
	if (key == record && st == 1 && check > 0) {
		split_bucket(table->table1, address, 1);
		address = rightmostnbits(table->table1->depth, hash);
		check = 0;
	}
	check++;

	if (!table->table1->buckets[address]->full) {
		table->table1->buckets[address]->key = key;
		table->table1->buckets[address]->full = true;
		table->table1->nkeys++;
		return true;
	} else {
		int64 old_key;
		old_key = table->table1->buckets[address]->key;
		table->table1->buckets[address]->key = key;
		return xuckoo_rehash_2(table, old_key, record, st, check);
	}
}

bool xuckoo_rehash_2(XuckooHashTable *table, int64 key, int64 record, int st, 
	int check) {
	int hash = h2(key);
	int address = rightmostnbits(table->table2->depth, hash);

	// st is start table
	// infinite loop occurs, split bucket
	if (key == record && st == 2 && check > 0) {
		split_bucket(table->table2, address, 2);
		address = rightmostnbits(table->table2->depth, hash);
		check = 0;
	}
	check++;

	if (!table->table2->buckets[address]->full) {
		table->table2->buckets[address]->key = key;
		table->table2->buckets[address]->full = true;
		table->table2->nkeys++;
		return true;
	} else {
		int64 old_key;
		old_key = table->table2->buckets[address]->key;
		table->table2->buckets[address]->key = key;
		return xuckoo_rehash_1(table, old_key, record, st, check);
	}
}

// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool xuckoo_hash_table_insert(XuckooHashTable *table, int64 key) {
	assert(table);
	// int start_time = clock();
	bool found;

	if (xuckoo_hash_table_lookup(table, key)) {
		// table->time = clock() - start_time;
		return false;
	}
	if (table->table1->nkeys <= table->table2->nkeys) {
		found = xuckoo_rehash_1(table, key, key, 1, 0);
	} else {
		found = xuckoo_rehash_2(table, key, key, 2, 0);
	}
	// table->time = clock() - start_time;
	return found;
}

/****************************************************************************/

// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool xuckoo_hash_table_lookup(XuckooHashTable *table, int64 key) {
	assert(table);
	// int start_time = clock();
	bool found = false;

	int ht1 = rightmostnbits(table->table1->depth, h1(key));
	int ht2 = rightmostnbits(table->table2->depth, h2(key));

	if (table->table1->buckets[ht1]->full) {
		if (table->table1->buckets[ht1]->key == key) {
			found = true;
		}
	} 
	if (table->table2->buckets[ht2]->full) {
		if (table->table2->buckets[ht2]->key == key) {
			found = true;
		}
	}
	// table->time += clock() - start_time;
	return found;
}

// print the contents of 'table' to stdout
void xuckoo_hash_table_print(XuckooHashTable *table) {
	assert(table != NULL);

	printf("--- table ---\n");

	// loop through the two tables, printing them
	InnerTable *innertables[2] = {table->table1, table->table2};
	int t;
	for (t = 0; t < 2; t++) {
		// print header
		printf("table %d\n", t+1);

		printf("  table:               buckets:\n");
		printf("  address | bucketid   bucketid [key]\n");
		
		// print table and buckets
		int i;
		for (i = 0; i < innertables[t]->size; i++) {
			// table entry
			printf("%9d | %-9d ", i, innertables[t]->buckets[i]->id);

			// if this is the first address at which a bucket occurs, print it
			if (innertables[t]->buckets[i]->id == i) {
				printf("%9d ", innertables[t]->buckets[i]->id);
				if (innertables[t]->buckets[i]->full) {
					printf("[%llu]", innertables[t]->buckets[i]->key);
				} else {
					printf("[ ]");
				}
			}

			// end the line
			printf("\n");
		}
	}
	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
void xuckoo_hash_table_stats(XuckooHashTable *table) {
	assert(table);

	printf("--- table stats ---\n");
	printf("table 1 size: %d\n", table->table1->size);
	printf("        keys: %d\n", table->table1->nkeys);
	printf("table 2 size: %d\n", table->table2->size);
	printf("        keys: %d\n", table->table2->nkeys);
	// float seconds = table->time * 1.0 / CLOCKS_PER_SEC;
	// printf("    CPU time spent: %.6f sec\n", seconds);
	printf("--- end stats ---\n");
}
