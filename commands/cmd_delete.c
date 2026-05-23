#include "../helpers.h"
#include "../types.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int cmd_delete(int argc, char *argv[])
{
	if (argc != 2) {
	    fprintf(stderr, "Usage: delete <name> <col>=<val>\n");
	    return 1;
	}

	const char* const name = argv[0];
	char path[MAX_PATH];
	fill_sch_path(path, name);

	int fds = open(path, O_RDWR, 0644);
	if (fds == -1) {
		perror("open .schema");
		return 1;
	}

	const char* const key = strtok(argv[1], "=");
	const char* const val = strtok(NULL, "=");
	if (!val) {
	    fprintf(stderr, "Need key=value pair\n");
	    return 1;
	}

	/* Overmap for simplicity */
	void *ms = mmap(NULL, 1UL << 30, PROT_READ | PROT_WRITE, MAP_SHARED, fds, 0);
	if (ms == MAP_FAILED) {
		perror("mmap");
		close(fds);
		return 1;
	}

	Schema *s = (Schema *)ms;

	if (s->magic != SCH_MAGIC) {
		fprintf(stderr, "Invalid magic for schema\n");
		munmap(ms, 1UL << 30);
		close(fds);
		return 1;
	}

	uint8_t ncols = s->ncols;
	uint8_t target_col;

	bool found = false;
	for (int i = 0; i < ncols; i++) {
		if (!strcmp(s->cols[i].name, key)) {
			found = true;
			target_col = i;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "No key '%s' in table '%s'\n", key, name);
		munmap(ms, 1UL << 30);
		close(fds);
		return 1;
	}

	ColType type = s->cols[target_col].type;
	intmax_t num;
	if (type == COL_INT) {
		num = strtoimax(val, NULL, 10);
		if (errno == ERANGE) {
			fprintf(stderr, "Integer %s is out of range\n", val);
			munmap(ms, 1UL << 30);
			close(fds);
		}
	} else if (type == COL_UNKNOWN) {
		fprintf(stderr, "Broken database\n");
		/* No need to care about graceful exit if it reached there */
		return 2;
	}

	fill_dat_path(path, name);
	int fdd = open(path, O_RDWR, 0644);
	if (fdd == -1) {
		perror("open .dat");
		munmap(ms, 1UL << 30);
		close(fds);
		return 1;
	}

	/* Overmap for simplicity */
	void *md = mmap(NULL, 1UL << 30, PROT_READ | PROT_WRITE, MAP_SHARED, fdd, 0);
	if (md == MAP_FAILED) {
		perror("mmap");
		munmap(ms, 1UL << 30);
		close(fds);
		close(fdd);
		return 1;
	}

	DataHeader *h = (DataHeader *)md;
	if (h->magic != DB_MAGIC) {
		fprintf(stderr, "Invalid magic for data\n");
		munmap(ms, 1UL << 30);
		munmap(md, 1UL << 30);
		close(fds);
		close(fdd);
		return 1;
	}

	uint8_t *data_ptr = (uint8_t *)md + sizeof(DataHeader) + target_col * FIELD_SIZE;

	int target_row;
	found = false;
	for (int i = 0; i < h->size; ++i) {
		if (type == COL_INT) {
			if (!memcmp((void *)data_ptr, (void *)&num, sizeof(intmax_t))) {
				found = true;
				target_row = i;
				break;
			}
		} else {
			if (!strcmp((char *)data_ptr, val)) {
				found = true;
				target_row = i;
				break;
			}
		}
		data_ptr += ncols * FIELD_SIZE;
	}

	if (!found) {
		fprintf(stderr, "No value '%s' in table '%s'\n", val, name);
		munmap(ms, 1UL << 30);
		munmap(md, 1UL << 30);
		close(fds);
		close(fdd);
		return 1;
	}

	data_ptr = (uint8_t *)md + sizeof(DataHeader) + target_row * ncols * FIELD_SIZE;
	if (target_row == (h->size - 1)) {
		memset((void *)data_ptr, 0, ncols * FIELD_SIZE);
	} else {
		uint8_t *last_ptr = (uint8_t *)md + sizeof(DataHeader) + h->size * ncols * FIELD_SIZE;
		uint8_t *next_ptr = data_ptr + ncols * FIELD_SIZE;
		memmove((void *)data_ptr, (void *)next_ptr, h->size * ncols * FIELD_SIZE);
		memset((void *)last_ptr, 0, ncols * FIELD_SIZE);
	}
	msync(md, sizeof(DataHeader) + h->size * ncols * FIELD_SIZE, MS_SYNC);
	--(h->size);

	printf("Removed row %d from table '%s'.\n", target_row, name);

	munmap(ms, 1UL << 30);
	munmap(md, 1UL << 30);
	close(fds);
	close(fdd);

	return 0;
}
