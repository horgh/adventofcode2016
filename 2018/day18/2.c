#define _POSIX_C_SOURCE 200810L

#include <assert.h>
#include <ctype.h>
#include <map.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

enum AcreType { Open, Trees, Lumber };

struct Position {
	enum AcreType type;
};

static void
print_map(struct Position * * const, int const);

static void
tick(struct Position * * const,
		struct Position * * const,
		int const);

static int
count(struct Position * * const,
		int const,
		enum AcreType const,
		int const,
		int const);

static char *
to_string(struct Position * * const, int const);

#define SZ 50

int
main(int const argc, char const * const * const argv)
{
	(void) argc;
	(void) argv;

	struct Position * * const map = calloc(SZ, sizeof(struct Position *));
	assert(map != NULL);
	for (size_t i = 0; i < SZ; i++) {
		map[i] = calloc(SZ, sizeof(struct Position));
		assert(map[i] != NULL);
	}
	int y = 0;
	while (1) {
		char buf[4096] = {0};
		if (fgets(buf, 4096, stdin) == NULL) {
			break;
		}
		trim_right(buf);

		char const * ptr = buf;
		int x = 0;
		while (*ptr != '\0') {
			if (*ptr == '.') {
				map[x][y].type = Open;
			} else if (*ptr == '|') {
				map[x][y].type = Trees;
			} else if (*ptr == '#') {
				map[x][y].type = Lumber;
			} else {
				assert(1 == 0);
			}
			ptr++;
			x++;
		}
		y++;
	}

	if (true) {
		print_map(map, y);
	}

	struct Position * * const map2 = calloc(SZ, sizeof(struct Position *));
	assert(map2 != NULL);
	for (size_t i = 0; i < SZ; i++) {
		map2[i] = calloc(SZ, sizeof(struct Position));
		assert(map2[i] != NULL);
	}

	//struct htable * const h = hash_init(1024);
	struct htable * h = hash_init(1024);
	assert(h != NULL);
	char * s = to_string(map, y);
	assert(hash_set(h, s, NULL));
	free(s);
	int const minutes = 1000000000;
	int interesting_minute = -1;
	int repeats = -1;
	for (int i = 0; i < minutes; i++) {
		if (i % 100000 == 0) {
			printf("%d\n", i);
			print_map(map, y);
		}
		tick(map, map2, y);

		s = to_string(map, y);
		if (hash_has_key(h, s)) {
			printf("found at %d\n", i);
			if (interesting_minute != -1) {
				repeats = i-interesting_minute;
				printf(" diff: %d\n", i-interesting_minute);
				free(s);
				break;
			}
			interesting_minute = i;
			assert(hash_free(h, NULL));
			h = hash_init(1024);
			assert(h != NULL);
			assert(hash_set(h, s, NULL));
			free(s);
			continue;
		}
		assert(hash_set(h, s, NULL));
		free(s);
	}
	assert(hash_free(h, NULL));

	printf("%d\n", repeats);
	int remaining = (minutes-(interesting_minute+1))%repeats;
	for (int i = 0; i < remaining; i++) {
		tick(map, map2, y);
	}

	int trees = 0;
	int lumber = 0;
	for (int y2 = 0; y2 < y; y2++) {
		for (int x = 0; x < y; x++) {
			if (map[x][y2].type == Trees) {
				trees++;
				continue;
			}
			if (map[x][y2].type == Lumber) {
				lumber++;
				continue;
			}
		}
	}

	for (size_t i = 0; i < SZ; i++) {
		free(map[i]);
		free(map2[i]);
	}
	free(map);
	free(map2);
	printf("%d\n", trees*lumber);
	return 0;
}

static void
print_map(struct Position * * const map, int const sz)
{
	for (int y = 0; y < sz; y++) {
		for (int x = 0; x < sz; x++) {
			if (map[x][y].type == Open) {
				printf(".");
				continue;
			}
			if (map[x][y].type == Trees) {
				printf("|");
				continue;
			}
			if (map[x][y].type == Lumber) {
				printf("#");
				continue;
			}
			assert(1 == 0);
		}
		printf("\n");
	}
}

static void
tick(struct Position * * const map,
		struct Position * * const map2,
		int const sz)
{
	for (size_t i = 0; i < SZ; i++) {
		memcpy(map2[i], map[i], SZ*sizeof(struct Position));
	}

	for (int y = 0; y < sz; y++) {
		for (int x = 0; x < sz; x++) {
			if (map2[x][y].type == Open) {
				int const trees = count(map2, sz, Trees, x, y);
				if (trees >= 3) {
					map[x][y].type = Trees;
				}
				continue;
			}
			if (map2[x][y].type == Trees) {
				int const lumber = count(map2, sz, Lumber, x, y);
				if (lumber >= 3) {
					map[x][y].type = Lumber;
				}
				continue;
			}
			if (map2[x][y].type == Lumber) {
				int const trees = count(map2, sz, Trees, x, y);
				int const lumber = count(map2, sz, Lumber, x, y);
				if (trees > 0 && lumber > 0) {
				} else {
					map[x][y].type = Open;
				}
				continue;
			}
			assert(1 == 0);
		}
	}
}

static int
count(struct Position * * const map,
		int const sz,
		enum AcreType const type,
		int const x,
		int const y)
{
	int c = 0;
	if (x-1 >= 0 && y-1 >= 0) {
		if (map[x-1][y-1].type == type) {
			c++;
		}
	}
	if (y-1 >= 0) {
		if (map[x][y-1].type == type) {
			c++;
		}
	}
	if (x+1 != sz && y-1 >= 0) {
		if (map[x+1][y-1].type == type) {
			c++;
		}
	}
	if (x-1 >= 0) {
		if (map[x-1][y].type == type) {
			c++;
		}
	}
	if (x+1 != sz) {
		if (map[x+1][y].type == type) {
			c++;
		}
	}
	if (x-1 >= 0 && y+1 != sz) {
		if (map[x-1][y+1].type == type) {
			c++;
		}
	}
	if (y+1 != sz) {
		if (map[x][y+1].type == type) {
			c++;
		}
	}
	if (x+1 != sz && y+1 != sz) {
		if (map[x+1][y+1].type == type) {
			c++;
		}
	}
	return c;
}

static char *
to_string(struct Position * * const map, int const sz)
{
	char * const s = calloc((size_t) (sz*sz+sz), sizeof(char));
	assert(s != NULL);
	int i = 0;
	for (int y = 0; y < sz; y++) {
		for (int x = 0; x < sz; x++) {
			if (map[x][y].type == Open) {
				s[i++] = '.';
				continue;
			}
			if (map[x][y].type == Trees) {
				s[i++] = '|';
				continue;
			}
			if (map[x][y].type == Lumber) {
				s[i++] = '#';
				continue;
			}
			assert(1 == 0);
		}
	}
	assert(strlen(s) == sz*sz);
	return s;
}
