#include "../helpers.h"
#include "../types.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int cmd_select(int argc, char *argv[]) {
	if (argc != 1) {
	    fprintf(stderr, "Usage: select <name>\n");
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

	/* Overmap for simplicity */
	void *ms = mmap(NULL, 1UL << 30, PROT_READ, MAP_SHARED, fds, 0);
	if (ms == MAP_FAILED) {
		perror("mmap");
		close(fds);
		return 1;
	}

	Schema *s = (Schema *)ms;

	fill_dat_path(path, name);
	int fdd = open(path, O_RDWR, 0644);
	if (fdd == -1) {
		perror("open .dat");
		munmap(ms, 1UL << 30);
		close(fds);
		return 1;
	}

	/* Overmap for simplicity */
	void *md = mmap(NULL, 1UL << 30, PROT_READ, MAP_SHARED, fdd, 0);
	if (md == MAP_FAILED) {
		perror("mmap");
		munmap(ms, 1UL << 30);
		close(fds);
		close(fdd);
		return 1;
	}

	DataHeader *h = (DataHeader *)md;

	uint8_t ncols = s->ncols;
	/* Print header */
	for (int i = 0; i < ncols; ++i)
		printf("%-20s", s->cols[i].name);
	printf("\n");
	for (int i = 0; i < ncols; i++)
		printf("%-20s", "--------------------");
	printf("\n");

	uint8_t *data_ptr = (uint8_t *)md + sizeof(DataHeader);
	for (int i = 0; i < h->size; ++i) {
		for (int j = 0; j < ncols; ++j) {
			ColType type = s->cols[j].type;
			if (type == COL_INT) {
				intmax_t val;
				memcpy(&val, data_ptr, sizeof(intmax_t));
				printf("%-20jd", val);
			} else if (type == COL_STR) {
				printf("%-20s", (char *)data_ptr);
			} else {
				fprintf(stderr, "Broken database\n");
				/* No need to care about graceful exit if it reached there */
				return 2;
			}
			data_ptr += FIELD_SIZE;
		}
		printf("\n");
	}

	for (int i = 0; i < ncols; i++)
		printf("%-20s", "--------------------");
	printf("\n");
	printf("\n");
	printf("Printed %d columnds %d rows from table '%s'.\n", ncols, h->size, name);

	munmap(ms, 1UL << 30);
	munmap(md, 1UL << 30);
	close(fds);
	close(fdd);

	return 0;
}
