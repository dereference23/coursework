#include "../helpers.h"
#include "../types.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>

static int check_name(const char* const name)
{
	char path[MAX_PATH];

	fill_sch_path(path, name);
	if (!access(path, F_OK)) {
		fprintf(stderr, "Schema for table '%s' already exists\n", name);
		return 1;
	}

	fill_dat_path(path, name);
	if (!access(path, F_OK)) {
		fprintf(stderr, "Table '%s' already exists\n", name);
		return 1;
	}

	return 0;
}

/* Create an empty data file */
static int dat_create(const char* const name, uint8_t ncols)
{
	char path[MAX_PATH];
	fill_dat_path(path, name);

	/* Align file size to page size for mmap() and calculate capacity */
	long page_size = sysconf(_SC_PAGESIZE);
	size_t data_size = sizeof(DataHeader) + ncols * FIELD_SIZE * INIT_CAP;
	size_t file_size = roundup(data_size, page_size);
	uint16_t capacity = file_size / (sizeof(DataHeader) + ncols * FIELD_SIZE);

	int fd = open(path, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		perror("create .dat");
		return 1;
	}

	if (ftruncate(fd, file_size) == -1) {
		perror("ftruncate");
		close(fd);
		return 1;
	}

	void *m = mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, fd, 0);
	if (m == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return 1;
	}

	DataHeader *h = (DataHeader *)m;
	*h = (DataHeader) {
		.magic = DB_MAGIC,
		.size = 0,
		.capacity = capacity,
	};

	msync(m, file_size, MS_SYNC);
	munmap(m, file_size);
	close(fd);

	return 0;
}

/* Create initial schema file */
static int sch_create(const char* const name, const Schema *s)
{
	char path[MAX_PATH];
	fill_sch_path(path, name);

	/* Align file size to page size for mmap() */
	long page_size = sysconf(_SC_PAGESIZE);
	size_t data_size = sizeof(Schema);
	size_t file_size = roundup(data_size, page_size);

	int fd = open(path, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		perror("create .schema");
		return 1;
	}

	if (ftruncate(fd, file_size) == -1) {
		perror("ftruncate");
		close(fd);
		return 1;
	}

	void *m = mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, fd, 0);
	if (m == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return -1;
	}

	memcpy(m, s, sizeof(Schema));

	msync(m, file_size, MS_SYNC);
	munmap(m, file_size);
	close(fd);
	return 0;
}

int cmd_create(int argc, char *argv[])
{
	/* Only creating a table is implemented */
	if (argc < 3 || strcmp(argv[0], "table") != 0) {
		fprintf(stderr, "Usage: create table <name> (<col1>, <col2>, ...)\n");
		return 1;
	}

	const char* const name = argv[1];
	size_t len = strlen(name);
	if (len >= MAX_TABLE_NAME) {
		fprintf(stderr, "Table name too long: max %zu, got %zu\n", MAX_TABLE_NAME-1, len);
		return 1;
	}

	if (!len) {
		fprintf(stderr, "Empty table name\n");
		return 1;
	}

	if (isspace(name[0])) {
		fprintf(stderr, "Invalid table name\n");
		return 1;
	}

	if (check_name(name))
		return 1;

	if (argv[2][0] != '(') {
		fprintf(stderr, "Expected opening '('\n");
		return 1;
	}

	uint8_t ncols = 1;
	/* If called with "(<col1>, <col2>, ...)" */
	if (argc == 3) {
		/* Skip opening '(' */
		char *tok, *valid_tok = strtok(argv[2]+1, ",");

		if (!valid_tok || valid_tok[0] == ')') {
			fprintf(stderr, "No columns specified\n");
			return 1;
		}

		for (;;) {
			tok = strtok(NULL, ",");
			if (!tok)
				break;
			valid_tok = tok;

			if (++ncols > MAX_COLS) {
				fprintf(stderr, "Too many columns: max %zu, got %u\n", MAX_COLS, ncols);
				return 1;
			}

			len = strlen(valid_tok);
			if (len >= MAX_COL_NAME) {
				fprintf(stderr, "Column name too long: max %zu, got %zu\n", MAX_COL_NAME-1, len);
				return 1;
			}
		}

		/* Special case */
		if (ncols == 1) {
			len = strlen(valid_tok);
			if (len >= MAX_COL_NAME) {
				fprintf(stderr, "Column name too long: max %zu, got %zu\n", MAX_COL_NAME-1, len);
				return 1;
			}
		}

		if (valid_tok[len-1] != ')') {
			fprintf(stderr, "Expected closing ')'\n");
			return 1;
		}
		valid_tok[len-1] = '\0';
	} else {
		/* If called without quoting */

		/* TODO */
		fprintf(stderr, "Got more arguments than expected\n");
		fprintf(stderr, "Try quoting the 3rd argument\n");
		return 1;
	}

	if (dat_create(name, ncols))
		return 1;

	Schema s = { .magic = SCH_MAGIC, .ncols = ncols };
	/* Extract column names */
	if (argc == 3) {
		/* Skip opening '(' */
		const char* pos = argv[2]+1;
		for (int i = 0; i < ncols; ++i) {
			s.cols[i].type = COL_UNKNOWN;
			if (isspace(pos[0]))
				++pos;
			len = strlcpy(s.cols[i].name, pos, MAX_COL_NAME);
			pos += len+1;
		}
	} else {
		/* TODO */
	}

	if (sch_create(name, &s))
		return 1;

	printf("Table '%s' created with %u column(s).\n", name, ncols);
	return 0;
}
