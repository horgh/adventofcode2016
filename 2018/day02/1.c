#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int count_occurrences(char const * const, char const);

int
main(int const argc, char const * const * const argv)
{
	(void)argc;
	(void)argv;

	int twos = 0;
	int threes = 0;
	while (1) {
		char buf[4096] = {0};
		if (fgets(buf, 4096, stdin) == NULL) {
			break;
		}

		char const * ptr = buf;
		bool has_two = false;
		bool has_three = false;
		while (*ptr != '\n' && *ptr != '\0') {
			int const count = count_occurrences(buf, *ptr);
			if (count == 2) {
				has_two = true;
			}
			if (count == 3) {
				has_three = true;
			}
			ptr++;
		}
		if (has_two) {
			twos++;
		}
		if (has_three) {
			threes++;
		}
	}

	printf("%d\n", twos * threes);
	return 0;
}

__attribute__((pure)) static int
count_occurrences(char const * const s, char const c)
{
	char const * ptr = s;
	int count = 0;
	while (*ptr != '\n' && *ptr != '\0') {
		if (*ptr == c) {
			count++;
		}
		ptr++;
	}
	return count;
}
