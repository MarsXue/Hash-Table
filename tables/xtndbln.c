/* * * * * * * * *
 * Dynamic hash table using extendible hashing with multiple keys per bucket,
 * resolving collisions by incrementally growing the hash table
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by Wenqing Xue <wenqingx@student.unimelb.edu.au>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "xtndbln.h"

// macro to calculate the rightmost n bits of a number x
#define rightmostnbits(n, x) (x) & ((1 << (n)) - 1)

// a bucket stores an array of keys
// it also knows how many bits are shared between possible keys, and the first 
// table address that references it
typedef struct xtndbln_bucket {
	int id;			// a unique id for this bucket, equal to the first address
					// in the table which points to it
	int depth;		// how many hash value bits are being used by this bucket
	int nkeys;		// number of keys currently contained in this bucket
	int64 *keys;	// the keys stored in this bucket
} Bucket;

// helper structure to store statistics gathered
typedef struct stats {
	int nbuckets;	// how many distinct buckets does the table point to
	int nkeys;		// how many keys are being stored in the table
	int time;		// how much CPU time has been used to insert/lookup keys
					// in this table
} Stats;

// a hash table is an array of slots pointing to buckets holding up to 
// bucketsize keys, along with some information about the number of hash value 
// bits to use for addressing
struct xtndbln_table {
	Bucket **buckets;	// array of pointers to buckets
	int size;			// how many entries in the table of pointers (2^depth)
	int depth;			// how many bits of the hash value to use (log2(size))
	int bucketsize;		// maximum number of keys per bucket
	Stats stats;		// collection of statistics about this hash table
};

/******************************* HELP FUNCTION *******************************/
static Bucket *new_bucket(int first_address, int depth, int bucketsize);
static void double_table(XtndblNHashTable * table);
static void reinsert_key(XtndblNHashTable *table, int64 key);
static void split_bucket(XtndblNHashTable *table, int address);
/****************************************************************************/

// create a new bucket with size of bucketsize
// the code was sourced from "xtndbl1.c"
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

// the code was sourced from "xtndbl1.c"
static void double_table(XtndblNHashTable * table) {
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
// since the array starts from 0, nkeys is used in insertion
static void reinsert_key(XtndblNHashTable *table, int64 key) {
	int address = rightmostnbits(table->depth, h1(key));
	int order = table->buckets[address]->nkeys;
	table->buckets[address]->keys[order] = key;
	table->buckets[address]->nkeys++;
}

// the code was sourced from "xtndbl1.c"
static void split_bucket(XtndblNHashTable *table, int address) {
	if (table->buckets[address]->depth == table->depth) {
		double_table(table);
	}
	Bucket *bucket = table->buckets[address];
	int depth = bucket->depth;
	int first_address = bucket->id;

	int new_depth = depth + 1;
	bucket->depth = new_depth;

	int new_first_address = 1 << depth | first_address;
	Bucket *newbucket = new_bucket(new_first_address, new_depth, table->bucketsize);
	table->stats.nbuckets++;

	int bit_address = rightmostnbits(depth, first_address);
	int suffix = (1 << depth) | bit_address;

	int maxprefix = 1 << (table->depth - new_depth);

	int prefix;
	for (prefix=0; prefix<maxprefix; prefix++) {
		int a = (prefix << new_depth) | suffix;
		table->buckets[a] = newbucket;
	}
	// record the nkeys
	int num = bucket->nkeys;
	int i;
	int64 key;
	// reset the nkeys
	bucket->nkeys = 0;

	// reinsert all the keys
	for (i=0; i<num; i++) {
		key = bucket->keys[i];
		reinsert_key(table, key);
	}
}

// initialise an extendible hash table with 'bucketsize' keys per bucket
// the code was sourced from "xtndbl1.c"
XtndblNHashTable *new_xtndbln_hash_table(int bucketsize) {
	XtndblNHashTable *table = malloc(sizeof *table);
	assert(table);

	table->size = 1;
	table->buckets = malloc(sizeof *table->buckets);
	assert(table->buckets);
	// initially the size of bucket is 1
	table->buckets[0] = new_bucket(0, 0, bucketsize);
	table->depth = 0;
	table->bucketsize = bucketsize;

	table->stats.nbuckets = 1;
	table->stats.nkeys = 0;	
	table->stats.time = 0;
	
	return table;
}

// free all memory associated with 'table'
// the code was sourced from "xtndbl1.c"
void free_xtndbln_hash_table(XtndblNHashTable *table) {
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


// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
// the code was sourced from "xtndbl1.c"
bool xtndbln_hash_table_insert(XtndblNHashTable *table, int64 key) {
	assert(table);
	int start_time = clock();

	int hash = h1(key);
	int address = rightmostnbits(table->depth, hash);

	if (xtndbln_hash_table_lookup(table, key)) {
		// table->stats.time += clock() - start_time;
		return false;
	}

	while (table->buckets[address]->nkeys >= table->bucketsize) {
		split_bucket(table, address);
		address = rightmostnbits(table->depth, hash);
	}
	reinsert_key(table, key);

	table->stats.nkeys++;
	table->stats.time += clock() - start_time;
	return true;
}


// lookup whether 'key' is inside 'table'
// returns true if found, false if not
// the code was sourced from "xtndbl1.c"
bool xtndbln_hash_table_lookup(XtndblNHashTable *table, int64 key) {
	assert(table);
	int start_time = clock();

	int i;
	int address = rightmostnbits(table->depth, h1(key));

	bool found = false;
	if (table->buckets[address]->nkeys != 0) {
		for (i=0; i<table->buckets[address]->nkeys; i++) {
			if (key == table->buckets[address]->keys[i]) {
				found = true;
			}
		}
	}
	table->stats.time += clock() - start_time;
	return found;
}


// print the contents of 'table' to stdout
void xtndbln_hash_table_print(XtndblNHashTable *table) {
	assert(table);
	printf("--- table size: %d\n", table->size);

	// print header
	printf("  table:               buckets:\n");
	printf("  address | bucketid   bucketid [key]\n");
	
	// print table and buckets
	int i;
	for (i = 0; i < table->size; i++) {
		// table entry
		printf("%9d | %-9d ", i, table->buckets[i]->id);

		// if this is the first address at which a bucket occurs, print it now
		if (table->buckets[i]->id == i) {
			printf("%9d ", table->buckets[i]->id);

			// print the bucket's contents
			printf("[");
			for(int j = 0; j < table->bucketsize; j++) {
				if (j < table->buckets[i]->nkeys) {
					printf(" %llu", table->buckets[i]->keys[j]);
				} else {
					printf(" -");
				}
			}
			printf(" ]");
		}
		// end the line
		printf("\n");
	}

	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
// the code was sourced from "xtndbl1.c"
void xtndbln_hash_table_stats(XtndblNHashTable *table) {
	printf("--- table stats ---\n");
	// print some stats about state of the table
	printf("current table size: %d\n", table->size);
	printf("    number of keys: %d\n", table->stats.nkeys);
	printf(" number of buckets: %d\n", table->stats.nbuckets);
	// also calculate CPU usage in seconds and print this
	float seconds = table->stats.time * 1.0 / CLOCKS_PER_SEC;
	printf("    CPU time spent: %.6f sec\n", seconds);
	printf("--- end stats ---\n");
}
