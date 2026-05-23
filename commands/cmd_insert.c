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

static inline ColType infer_type(const char* v) {
	/* Signed integer? */
	if (*v == '-')
		++v;

	/* Empty string */
	if (*v == '\0')
		return COL_STR;

	for (const char* p = v; *p != '\0'; ++p)
		if (!isdigit(*p))
			return COL_STR;

	return COL_INT;
}

int cmd_insert(int argc, char *argv[])
{
	if (argc < 2) {
	    fprintf(stderr, "Usage: insert <name> <val1> <val2> ...\n");
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

	int ncols = argc - 1;
	if (ncols != s->ncols) {
		fprintf(stderr, "Column count mismatch: expected %u, got %u\n", s->ncols, ncols);
		perror("mmap");
		munmap(ms, 1UL << 30);
		close(fds);
		return 1;
	}

	/* Infer column types from the first INSERT */
	bool clean = true;
	for (int i = 0; i < ncols; i++) {
		if (s->cols[i].type == COL_UNKNOWN)
			s->cols[i].type = infer_type(argv[1 + i]);
		else {
			clean = false;
			break;
		}
	}

	if (clean)
		msync(ms, sizeof(Schema), MS_SYNC);

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

	if (h->capacity == h->size) {
		/* TODO: grow capacity */
		fprintf(stderr, "Not implemented\n");
		return 1;
	}

	uint8_t *data_ptr = (uint8_t *)md + sizeof(DataHeader) + h->size * ncols * FIELD_SIZE;
	for (int i = 0; i < ncols; i++) {
		ColType type = s->cols[i].type;
		if (type == COL_INT) {
			intmax_t num = strtoimax(argv[1 + i], NULL, 10);
			if (errno == ERANGE) {
				fprintf(stderr, "Integer %s is out of range\n", argv[1 + i]);
				munmap(ms, 1UL << 30);
				munmap(md, 1UL << 30);
				close(fds);
				close(fdd);
			}
			*(intmax_t *)data_ptr = num;
		} else if (type == COL_STR)
			strncpy((void *)data_ptr, argv[1 + i], FIELD_SIZE);
		else {
			fprintf(stderr, "Broken database\n");
			/* No need to care about graceful exit if it reached there */
			return 2;
		}
		data_ptr += FIELD_SIZE;
	}
	++(h->size);
	msync(md, sizeof(DataHeader) + h->size * ncols * FIELD_SIZE, MS_SYNC);

	printf("Inserted to table '%s' successfully.\n", name);

	munmap(ms, 1UL << 30);
	munmap(md, 1UL << 30);
	close(fds);
	close(fdd);

	return 0;
}
