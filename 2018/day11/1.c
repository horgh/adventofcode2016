#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#define SZ 300

int
main(int const argc, char const * const * const argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <serial>\n", argv[0]);
		return 1;
	}
	int const serial = atoi(argv[1]);

	int grid[SZ][SZ] = {0};

	for (int y = 0; y < SZ; y++) {
		for (int x = 0; x < SZ; x++) {
			int const rack_id = x + 1 + 10;
			int power_level = rack_id * (y + 1);
			power_level += serial;
			power_level *= rack_id;
			if (power_level < 100) {
				power_level = 0;
			} else {
				power_level = power_level % 1000 / 100;
			}
			power_level -= 5;
			grid[x][y] = power_level;
		}
	}

	int max_power = 0;
	int max_x = 0;
	int max_y = 0;
	for (int y = 0; y < SZ - 2; y++) {
		for (int x = 0; x < SZ - 2; x++) {
			int power = 0;
			for (int yy = y; yy < y + 3; yy++) {
				for (int xx = x; xx < x + 3; xx++) {
					power += grid[xx][yy];
				}
			}
			if (power <= max_power) {
				continue;
			}
			max_power = power;
			max_x = x;
			max_y = y;
		}
	}

	printf("%d,%d\n", max_x + 1, max_y + 1);
	return 0;
}
