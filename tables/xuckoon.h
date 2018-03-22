/* * * * * * * * *
 * Multi-key Extendible Cuckoo Hashing
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by Wenqing Xue <wenqingx@student.unimelb.edu.au>
 */

#ifndef XUCKOON_H
#define XUCKOON_H

#include <stdbool.h>
#include "../inthash.h"

typedef struct xuckoon_table XuckoonHashTable;

XuckoonHashTable *new_xuckoon_hash_table(int bucketsize);

void free_xuckoon_hash_table(XuckoonHashTable *table);

bool xuckoon_hash_table_insert(XuckoonHashTable *table, int64 key);

bool xuckoon_hash_table_lookup(XuckoonHashTable *table, int64 key);

void xuckoon_hash_table_print(XuckoonHashTable *table);

void xuckoon_hash_table_stats(XuckoonHashTable *table);

#endif