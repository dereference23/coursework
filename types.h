#pragma once

#include <stdint.h>
#include <string.h>

#define DB_MAGIC	0xDBDBu
#define SCH_MAGIC	0xDBDDu

#define MAX_TABLE_NAME	16LU /* max table name length */
#define MAX_COL_NAME	16LU /* max column name length */
#define MAX_COLS	16LU /* max columns per table */
#define FIELD_SIZE	16LU /* bytes per stored field value */
#define INIT_CAP	16LU /* initial capacity */

#define MAX_PATH	(MAX_TABLE_NAME+strlen(".schema"))

typedef enum {
	COL_UNKNOWN,
	COL_INT,
	COL_STR,
} ColType;

typedef struct {
	char name[MAX_COL_NAME];
	ColType type;
} Column;

typedef struct {
	uint16_t magic;
	uint8_t ncols;
	Column cols[MAX_COLS];
} Schema;

typedef struct {
	uint16_t magic;
	uint16_t size;
	uint16_t capacity;
} DataHeader;
