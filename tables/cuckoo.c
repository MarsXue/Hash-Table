/* * * * * * * * *
 * Dynamic hash table using cuckoo hashing, resolving collisions by switching
 * keys between two tables with two separate hash functions
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by Wenqing Xue <wenqingx@student.unimelb.edu.au>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "cuckoo.h"

// an inner table represents one of the two internal tables for a cuckoo
// hash table. it stores two parallel arrays: 'slots' for storing keys and
// 'inuse' for marking which entries are occupied
typedef struct inner_table {
	int64 *slots;	// array of slots holding keys
	bool  *inuse;	// is this slot in use or not?
} InnerTable;

typedef struct stats {
	int time;
} Stats;

// a cuckoo hash table stores its keys in two inner tables
struct cuckoo_table {
	InnerTable *table1; // first table
	InnerTable *table2; // second table
	int size;			// size of each table
	int load;			// number of keys
	Stats stats;
};

/******************************* HELP FUNCTION *******************************/
static void initialise_inner_table(InnerTable *table, int size);
static void initialise_cuckoo_table(CuckooHashTable *table, int size);
static void double_table(CuckooHashTable *table);
void free_inner_table(InnerTable *table);
bool cuckoo_rehash_1(CuckooHashTable *table, int64 key, int64 record);
bool cuckoo_rehash_2(CuckooHashTable *table, int64 key, int64 record);
/****************************************************************************/

// initialise a inner table with 'size' slots
static void initialise_inner_table(InnerTable *table, int size) {
	assert(table);
	table->slots = malloc((sizeof *table->slots) * size);
	assert(table->slots);
	table->inuse = malloc((sizeof *table->inuse) * size);
	assert(table->inuse);

	int i;
	for (i=0; i<size; i++) {
		table->inuse[i] = false;
	}
}

// initialise a cuckoo hash table with 'size' slots
static void initialise_cuckoo_table(CuckooHashTable *table, int size) {
	assert(table);
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	table->size = size;
	table->load = 0;

	table->stats.time = 0;

	table->table1 = malloc(sizeof *table->table1);
	assert(table->table1);
	initialise_inner_table(table->table1, size);

	table->table2 = malloc(sizeof *table->table2);
	assert(table->table2);
	initialise_inner_table(table->table2, size);
}

// double size of the cuckoo hash table
static void double_table(CuckooHashTable *table) {
	int oldsize = table->size;
	int newsize = oldsize * 2;
	assert(newsize < MAX_TABLE_SIZE && "error: table has grown too large!");

	int64 *oldslots1 = table->table1->slots;
	int64 *oldslots2 = table->table2->slots;
	bool *oldinuse1 = table->table1->inuse;
	bool *oldinuse2 = table->table2->inuse;

	free(table->table1);
	free(table->table2);

	initialise_cuckoo_table(table, newsize);
	// after initialise the cuckoo table with double size
	// reinsert all the values in new cuckoo table
	int i;
	for (i=0; i<oldsize; i++) {
		if (oldinuse1[i]) {
			cuckoo_hash_table_insert(table, oldslots1[i]);
		}
		if (oldinuse2[i]) {
			cuckoo_hash_table_insert(table, oldslots2[i]);
		}
	}
	free(oldslots1);
	free(oldslots2);
	free(oldinuse1);
	free(oldinuse2);
}

// initialise a cuckoo hash table
CuckooHashTable *new_cuckoo_hash_table(int size) {
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	CuckooHashTable *table = malloc(sizeof *table);
	assert(table);

	initialise_cuckoo_table(table, size);
	return table;
}

/****************************************************************************/

// free all the memory accociated with inner table
void free_inner_table(InnerTable *table) {
	assert(table);
	free(table->slots);
	free(table->inuse);
	free(table);
}

// free all memory associated with cuckoo hash table
void free_cuckoo_hash_table(CuckooHashTable *table) {
	assert(table);
	free_inner_table(table->table1);
	free_inner_table(table->table2);
	free(table);
}

/****************************************************************************/

// rehash the key in table 1
bool cuckoo_rehash_1(CuckooHashTable *table, int64 key, int64 record) {

	int ht1 = h1(key) % table->size;
	int64 old_key;

	if (!table->table1->inuse[ht1]) {
		// if not inuse
		table->table1->slots[ht1] = key;
		table->table1->inuse[ht1] = true;
		return true;
	} else {
		// if already inuse
		// replace the old key in ht1 position with the inserted key
		old_key = table->table1->slots[ht1];
		table->table1->slots[ht1] = key;
		// rehash the old key in table 2
		return cuckoo_rehash_2(table, old_key, record);
	}
}

// rehahs the key in table 2
bool cuckoo_rehash_2(CuckooHashTable *table, int64 key, int64 record) {
	if (key == record) {
		// infinite loop
		// once the original key occurs in rehash function of table 2
		return false;
	}
	int ht2 = h2(key) % table->size;
	int64 old_key;

	if (!table->table2->inuse[ht2]) {
		// if not use
		table->table2->slots[ht2] = key;
		table->table2->inuse[ht2] = true;
		return true;
	} else {
		// if already use
		// replace the old key in ht2 position with the inserted key
		old_key = table->table2->slots[ht2];
		table->table2->slots[ht2] = key;
		// rehash the old key in table 1
		return cuckoo_rehash_1(table, old_key, record);
	}	
}

// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool cuckoo_hash_table_insert(CuckooHashTable *table, int64 key) {
	assert(table);
	int start_time = clock();

	if (cuckoo_hash_table_lookup(table, key)) {
		// check if it is in table
		table->stats.time += clock() - start_time;
		return false;
	}
	// after lookup, the key must be inserted
	table->load++;

	if (cuckoo_rehash_1(table, key, key)) {
		// if rehash recursion is valid
		table->stats.time += clock() - start_time;
		return true; 
	} else {
		// infinite loop occurs
		// double the table size
		double_table(table);
		return cuckoo_hash_table_insert(table, key);
	}
}

/****************************************************************************/
// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool cuckoo_hash_table_lookup(CuckooHashTable *table, int64 key) {
	assert(table);
	int start_time = clock();
	bool found = false;

	int ht1 = h1(key) % table->size;
	int ht2 = h2(key) % table->size;
	// key occurs only in the corresponding ht1 & ht2 position
	if (table->table1->slots[ht1] == key || table->table2->slots[ht2] == key) {
		found = true;
	}
	table->stats.time += clock() - start_time;
	return found;
}

/****************************************************************************/
// print the contents of 'table' to stdout
void cuckoo_hash_table_print(CuckooHashTable *table) {
	assert(table);
	printf("--- table size: %d\n", table->size);

	// print header
	printf("                    table one         table two\n");
	printf("                  key | address     address | key\n");
	
	// print rows of each table
	int i;
	for (i = 0; i < table->size; i++) {

		// table 1 key
		if (table->table1->inuse[i]) {
			printf(" %20llu ", table->table1->slots[i]);
		} else {
			printf(" %20s ", "-");
		}

		// addresses
		printf("| %-9d %9d |", i, i);

		// table 2 key
		if (table->table2->inuse[i]) {
			printf(" %llu\n", table->table2->slots[i]);
		} else {
			printf(" %s\n",  "-");
		}
	}

	// done!
	printf("--- end table ---\n");
}

/****************************************************************************/
// print some statistics about 'table' to stdout
void cuckoo_hash_table_stats(CuckooHashTable *table) {
	assert(table);
	printf("--- table stats ---\n");
	printf("current size: %d * 2 slots\n", table->size);
	printf("current load: %d items\n", table->load);
	printf(" load factor: %.3f%%\n", table->load * 100.0 / (table->size * 2));
	float seconds = table->stats.time * 1.0 / CLOCKS_PER_SEC;
	printf("    CPU time spent: %.6f sec\n", seconds);
	printf("--- end stats ---\n");
}
