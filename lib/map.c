#include "map.h"

#include "siphash.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_SIP_HASH

#define SIP_KEY_LEN 16

const uint8_t sip_key[SIP_KEY_LEN] = {0x00,
		0x01,
		0x02,
		0x03,
		0x04,
		0x05,
		0x06,
		0x07,
		0x08,
		0x09,
		0x0A,
		0x0B,
		0x0C,
		0X0D,
		0x0E,
		0x0F};

static int __hasher(char const * const, size_t const, size_t const);
static bool __hash_set(struct htable * const,
		int const,
		void const * const,
		size_t const,
		void * const);
static bool __hash_delete(struct htable * const,
		int const,
		void const * const,
		size_t const,
		void(void * const));
static void __hash_counter(struct hnode const * const, void * const);

struct htable *
hash_init(size_t const n)
{
	struct htable * const h = calloc(1, sizeof(struct htable));
	if (!h) {
		return NULL;
	}

	h->size = n;

	h->nodes = calloc(n, sizeof(struct htable *));
	if (!h->nodes) {
		free(h);
		return NULL;
	}

	return h;
}

// Copy the hash table.
//
// We make copies of everything except values.
struct htable *
hash_copy(struct htable const * const h)
{
	if (!h) {
		return NULL;
	}

	struct htable * const h2 = calloc(1, sizeof(struct htable));
	if (!h2) {
		return NULL;
	}

	h2->size = h->size;

	h2->nodes = calloc(h2->size, sizeof(struct htable *));
	if (!h2->nodes) {
		hash_free(h2, NULL);
		return NULL;
	}

	for (size_t i = 0; i < h->size; i++) {
		struct hnode const * nptr = *(h->nodes + i);
		struct hnode * nptr2 = NULL;

		while (nptr) {
			// Copy the node.

			struct hnode * node = calloc(1, sizeof(struct hnode));
			if (!node) {
				hash_free(h2, NULL);
				return NULL;
			}

			node->key = calloc(1, nptr->key_size);
			if (!node->key) {
				hash_free(h2, NULL);
				free(node);
				return NULL;
			}
			memcpy(node->key, nptr->key, nptr->key_size);

			node->key_size = nptr->key_size;

			node->value = nptr->value;

			// Hook it in.

			if (nptr2) {
				nptr2->next = node;
			} else {
				h2->nodes[i] = node;
			}
			nptr2 = node;

			nptr = nptr->next;
		}
	}

	return h2;
}

__attribute__((pure)) static int
__hasher(char const * const key, size_t const keylen, size_t const n)
{
	if (!key || keylen == 0) {
		return 0;
	}

	int actual_hash = 0;

#ifdef USE_SIP_HASH
	uint64_t hash = 0;
	siphash((uint8_t const * const)key,
			(uint64_t)keylen + 1,
			sip_key,
			(uint8_t *)&hash,
			8);

	actual_hash = (int)hash % (int)n;
	actual_hash = abs(actual_hash);
#else
	int hash = 0;
	int const mult = 31;

	for (size_t i = 0; i < keylen; i++) {
		hash += mult * hash + key[i];
	}
	hash = abs(hash);

	actual_hash = hash % (int)n;
#endif

#ifdef TEST_MAP
	printf("%s hashes to %d\n", key, actual_hash);
#endif

	return actual_hash;
}

// Set the key to the value.
//
// We make a copy of the key.
bool
hash_set(struct htable * const h, char const * const key, void * const value)
{
	size_t const keylen = strlen(key);
	if (!h || !key || keylen == 0) {
		fprintf(stderr, "hash_set: %s: ", strerror(EINVAL));
		if (!h) {
			fprintf(stderr, "hash argument missing\n");
		}
		if (!key) {
			fprintf(stderr, "key argument missing\n");
		}
		if (keylen == 0) {
			fprintf(stderr, "key is blank\n");
		}
		return false;
	}

	int const hash = __hasher(key, keylen, h->size);

	return __hash_set(h, hash, key, keylen + 1, value);
}

bool
hash_set_i(struct htable * const h, int const key, void * const value)
{
	if (!h) {
		return false;
	}

	int const hash = abs(key) % (int)h->size;

	return __hash_set(h, hash, &key, sizeof(int), value);
}

static bool
__hash_set(struct htable * const h,
		int const hash,
		void const * const key,
		size_t const key_size,
		void * const value)
{
	if (!h || !key) {
		fprintf(stderr, "__hash_set: %s\n", strerror(EINVAL));
		return false;
	}

	struct hnode * nptr = h->nodes[hash];

	if (!nptr) {
		nptr = calloc(1, sizeof(struct hnode));
		if (!nptr) {
			return false;
		}

		nptr->key = calloc(1, key_size);
		if (!nptr->key) {
			free(nptr);
			return false;
		}
		memcpy(nptr->key, key, key_size);

		nptr->key_size = key_size;

		nptr->value = value;

		h->nodes[hash] = nptr;
		return true;
	}

	struct hnode * prev = NULL;

	while (nptr) {
		if (memcmp(nptr->key, key, key_size) == 0) {
			// TODO: Likely memory leak here. Need to clean up what we clobber.
			nptr->value = value;
			return true;
		}

		prev = nptr;
		nptr = nptr->next;
	}

	h->collisions++;
#ifdef MAP_DEBUG
	if (h->collisions % 100 == 0) {
		printf("collisions %zu...\n", h->collisions);
	}
#endif

	nptr = calloc(1, sizeof(struct hnode));
	if (!nptr) {
		return false;
	}

	nptr->key = calloc(1, key_size);
	if (!nptr->key) {
		free(nptr);
		return false;
	}
	memcpy(nptr->key, key, key_size);

	nptr->key_size = key_size;

	nptr->value = value;

	prev->next = nptr;
	return true;
}

__attribute__((pure)) void *
hash_get(struct htable const * const h, char const * const key)
{
	size_t const keylen = strlen(key);
	if (!h || !key || keylen == 0) {
		return NULL;
	}

	int const hash = __hasher(key, keylen, h->size);

	struct hnode const * nptr = h->nodes[hash];

	while (nptr) {
		if (strcmp(nptr->key, key) == 0) {
			return nptr->value;
		}

		nptr = nptr->next;
	}

	return NULL;
}

__attribute__((pure)) void *
hash_get_i(struct htable const * const h, int const key)
{
	if (!h) {
		return NULL;
	}

	int const hash = abs(key) % (int)h->size;
	struct hnode const * nptr = h->nodes[hash];
	while (nptr) {
		int const key2 = *((int *)nptr->key);
		if (key2 == key) {
			return nptr->value;
		}

		nptr = nptr->next;
	}

	return NULL;
}

__attribute__((pure)) bool
hash_has_key(struct htable const * const h, char const * const key)
{
	size_t const keylen = strlen(key);
	if (!h || !key || keylen == 0) {
		return false;
	}

	int const hash = __hasher(key, keylen, h->size);

	struct hnode const * nptr = h->nodes[hash];

	while (nptr) {
		if (strcmp(nptr->key, key) == 0) {
			return true;
		}

		nptr = nptr->next;
	}

	return false;
}

__attribute__((pure)) bool
hash_has_key_i(struct htable const * const h, int const key)
{
	if (!h) {
		return false;
	}

	int const hash = abs(key) % (int)h->size;
	struct hnode const * nptr = h->nodes[hash];
	while (nptr) {
		int const key2 = *((int *)nptr->key);
		if (key2 == key) {
			return true;
		}

		nptr = nptr->next;
	}

	return false;
}

bool
hash_delete(
		struct htable * const h, char const * const key, void fn(void * const))
{
	size_t const keylen = strlen(key);
	if (!h || !key || keylen == 0) {
		return false;
	}

	int const hash = __hasher(key, keylen, h->size);
	return __hash_delete(h, hash, key, keylen + 1, fn);
}

bool
hash_delete_i(struct htable * const h, int const key, void fn(void * const))
{
	int const hash = abs(key) % (int)h->size;
	return __hash_delete(h, hash, &key, sizeof(int), fn);
}

static bool
__hash_delete(struct htable * const h,
		int const hash,
		void const * const key,
		size_t const key_size,
		void fn(void * const))
{
	struct hnode * nptr = *(h->nodes + hash);

	if (!nptr) {
		return false;
	}

	if (nptr->next) {
		h->collisions--;
	}

	struct hnode * prev = NULL;

	while (nptr) {
		if (memcmp(nptr->key, key, key_size) == 0) {
			if (prev) {
				prev->next = nptr->next;

				free(nptr->key);

				if (fn) {
					fn(nptr->value);
				}

				free(nptr);
				return true;
			}

			h->nodes[hash] = nptr->next;

			free(nptr->key);

			if (fn) {
				fn(nptr->value);
			}

			free(nptr);
			return true;
		}

		prev = nptr;
		nptr = nptr->next;
	}

	return false;
}

// Retrieve all keys in the hash.
//
// Free the returned memory with hash_free_keys().
void **
hash_get_keys(struct htable const * const h)
{
	if (!h) {
		return NULL;
	}

	// It's possible this is not sufficiently large if we have any collisions.
	// Possibly we could calculate exactly the size we need ahead of time.
	size_t array_size = h->size;

	void ** keys = calloc(array_size, sizeof(void *));
	if (!keys) {
		return NULL;
	}

	size_t keys_i = 0;

	for (size_t i = 0; i < h->size; i++) {
		const struct hnode * nptr = *(h->nodes + i);

		while (nptr) {
			// Terminate with null at end always.
			if (keys_i == array_size - 1) {
				// TODO: overflow
				size_t const new_array_size = array_size * 2;

				// TODO: overflow?
				void ** const new_keys = realloc(keys, new_array_size * sizeof(void *));
				if (!new_keys) {
					free(keys);
					return NULL;
				}

				memset(new_keys + array_size, 0, array_size * sizeof(void *));

				array_size = new_array_size;
				keys = new_keys;
			}

			void * const key = calloc(1, nptr->key_size);
			if (!key) {
				hash_free_keys(keys);
				return NULL;
			}
			memcpy(key, nptr->key, nptr->key_size);

			keys[keys_i] = key;
			keys_i++;

			nptr = nptr->next;
		}
	}

	return keys;
}

void
hash_free_keys(void ** const keys)
{
	if (!keys) {
		return;
	}

	for (size_t i = 0; keys[i]; i++) {
		free(keys[i]);
	}

	free(keys);
}

// p gets passed to each node. You can use it to carry around state.
bool
hash_iterate(struct htable const * const h,
		void fn(struct hnode const * const, void * const),
		void * const p)
{
	if (!h || !fn) {
		return false;
	}

	for (size_t i = 0; i < h->size; i++) {
		struct hnode const * nptr = *(h->nodes + i);

		while (nptr) {
			fn(nptr, p);
			nptr = nptr->next;
		}
	}

	return true;
}

__attribute__((pure)) int
hash_count_elements(struct htable const * const h)
{
	int i = 0;
	if (!hash_iterate(h, __hash_counter, &i)) {
		return -1;
	}
	return i;
}

static void
__hash_counter(struct hnode const * const h, void * const p)
{
	(void)h;

	int * const i = p;

	*i += 1;
}

bool
hash_free(struct htable * const h, void fn(void * const))
{
	if (!h) {
		return false;
	}

	if (h->nodes) {
		for (size_t i = 0; i < h->size; i++) {
			struct hnode * nptr = *(h->nodes + i);

			while (nptr) {
				struct hnode * next = nptr->next;

				if (nptr->key) {
					free(nptr->key);
				}

				if (fn) {
					fn(nptr->value);
				}

				free(nptr);

				nptr = next;
			}
		}

		free(h->nodes);
	}

	free(h);

	return true;
}

#ifdef TEST_MAP

#include <assert.h>

static void __get_value(const struct hnode * const, void * const);
static void test_hash_get_keys(void);
static void test_int_keys(void);

int
main(int const argc, char const * const * const argv)
{
	(void)argc;
	(void)argv;

	struct htable * h = NULL;

	size_t size = 100;

	void * found_value = NULL;
	int found_int = 0;

	h = hash_init(size);
	assert(h != NULL);

	// blah => 5

	const char * key1 = "blah";
	int * value1 = calloc(1, sizeof(int));
	assert(value1 != NULL);
	*value1 = 5;
	assert(hash_set(h, key1, value1));

	found_value = hash_get(h, key1);
	found_int = *(int *)found_value;
	printf("found %d\n", found_int);
	assert(found_int == *value1);

	// blah2 => 10

	const char * key2 = "blah2";
	int * value2 = calloc(1, sizeof(int));
	assert(value2 != NULL);
	*value2 = 10;
	assert(hash_set(h, key2, value2));

	found_value = hash_get(h, key2);
	found_int = *(int *)found_value;
	printf("found %d\n", found_int);
	assert(found_int == *value2);

	found_value = hash_get(h, key1);
	found_int = *(int *)found_value;
	printf("found %d\n", found_int);
	assert(found_int == *value1);

	// Test hash_iterate
	int i = -1;
	hash_iterate(h, __get_value, &i);
	// 10 happens to have highest hash.
	assert(i == 10);

	// Test hash_get_keys
	void ** const keys = hash_get_keys(h);

	for (size_t j = 0; keys[j]; j++) {
		char const * const key = keys[j];
		printf("key: %s\n", key);
	}

	hash_free_keys(keys);

	// Test hash_count_elements
	assert(hash_count_elements(h) == 2);

	assert(hash_delete(h, "blah2", free));
	assert(hash_count_elements(h) == 1);

	// Test hash_has_key
	assert(hash_has_key(h, "blah"));
	assert(!hash_has_key(h, "blah2"));

	// Test hash_copy
	struct htable * h2 = hash_copy(h);
	assert(h2 != NULL);
	assert(hash_count_elements(h2) == hash_count_elements(h));
	assert(hash_has_key(h2, "blah"));
	assert(!hash_has_key(h2, "blah2"));
	assert(hash_free(h2, NULL));

	assert(hash_free(h, free));

	test_hash_get_keys();
	test_int_keys();
}

static void
__get_value(const struct hnode * const node, void * const p)
{
	int * value = node->value;

	int * ret = p;

	*ret = *value;
}

static void
test_hash_get_keys(void)
{
	{
		// Test reallocation logic. This happens when we have to reallocate the key
		// array due to not having created a large enough one up front. We can
		// cause this by ensuring we have a collision as the initial size of the
		// keys array is based on the hash size.

		size_t const size = 1;
		struct htable * const h = hash_init(size);
		assert(h != NULL);

		char const * const key0 = "abc";
		assert(hash_set(h, key0, NULL));

		char const * const key1 = "def";
		assert(hash_set(h, key1, NULL));

		void ** const keys = hash_get_keys(h);
		assert(keys != NULL);

		assert(strcmp(keys[0], "abc") == 0);
		assert(strcmp(keys[1], "def") == 0);
		assert(keys[2] == NULL);

		hash_free_keys(keys);

		assert(hash_free(h, NULL));
	}
}

static void
test_int_keys(void)
{
	struct htable * const h = hash_init(1024);
	assert(h != NULL);

	for (int i = 0; i < 102400; i++) {
		assert(!hash_has_key_i(h, i));

		int * const j = calloc(1, sizeof(int));
		assert(j != NULL);
		*j = i;

		assert(hash_set_i(h, i, j));

		assert(hash_has_key_i(h, i));

		void const * const v = hash_get_i(h, i);
		int const j2 = *((int const *)v);
		assert(j2 == i);

		assert(hash_delete_i(h, i, free));

		void const * const v2 = hash_get_i(h, i);
		assert(v2 == NULL);
	}

	assert(hash_free(h, free));
}

#endif
