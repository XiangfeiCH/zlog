/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2011 by Hardy Simpson <HardySimpson1984@gmail.com>
 *
 * The zlog Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The zlog Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the zlog Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "zc_defs.h"
#include "zc_hashtable.h"

zc_hashtable_t *zc_hashtable_new(size_t a_size,
				zc_hashtable_hash_fn hash_fn, zc_hashtable_equal_fn equal_fn,
				zc_hashtable_del_fn key_del_fn, zc_hashtable_del_fn value_del_fn)
{
	zc_hashtable_t *a_table;

	zc_assert(a_size, NULL);
	zc_assert(hash_fn, NULL);
	zc_assert(equal_fn, NULL);

	a_table = calloc(1, sizeof(*a_table));
	if (!a_table) {
		zc_error("calloc fail, errno[%d]", errno);
		return NULL;
	}

	a_table->tab = calloc(a_size, sizeof(*(a_table->tab)));
	if (!a_table->tab) {
		zc_error("calloc fail, errno[%d]", errno);
		free(a_table);
		return NULL;
	}
	a_table->tab_size = a_size;

	a_table->nelem = 0;
	a_table->hash_fn = hash_fn;
	a_table->equal_fn = equal_fn;

	/* these two could be NULL */
	a_table->key_del_fn = key_del_fn;
	a_table->value_del_fn = value_del_fn;

	return a_table;
}

void zc_hashtable_del(zc_hashtable_t * a_table)
{
	size_t i;
	zc_hashtable_entry_t *p;
	zc_hashtable_entry_t *q;

	zc_assert(a_table,);

	for (i = 0; i < a_table->tab_size; i++) {
		for (p = (a_table->tab)[i]; p; p = q) {
			q = p->next;
			if (a_table->key_del_fn) {
				a_table->key_del_fn(p->key);
			}
			if (a_table->value_del_fn) {
				a_table->value_del_fn(p->value);
			}
			free(p);
		}
	}
	if (a_table->tab)
		free(a_table->tab);
	free(a_table);

	return;
}

void zc_hashtable_clean(zc_hashtable_t * a_table)
{
	size_t i;
	zc_hashtable_entry_t *p;
	zc_hashtable_entry_t *q;

	zc_assert(a_table,);

	for (i = 0; i < a_table->tab_size; i++) {
		for (p = (a_table->tab)[i]; p; p = q) {
			q = p->next;
			if (a_table->key_del_fn) {
				a_table->key_del_fn(p->key);
			}
			if (a_table->value_del_fn) {
				a_table->value_del_fn(p->value);
			}
			free(p);
		}
		(a_table->tab)[i] = NULL;
	}
	a_table->nelem = 0;
	return;
}

static int zc_hashtable_rehash(zc_hashtable_t * a_table)
{
	size_t i;
	size_t j;
	size_t tab_size;
	zc_hashtable_entry_t **tab;
	zc_hashtable_entry_t *p;
	zc_hashtable_entry_t *q;

	zc_assert(a_table, -1);

	tab_size = 2 * a_table->tab_size;
	tab = calloc(tab_size, sizeof(*tab));
	if (!tab) {
		zc_error("calloc fail, errno[%d]", errno);
		return -1;
	}

	for (i = 0; i < a_table->tab_size; i++) {
		for (p = (a_table->tab)[i]; p; p = q) {
			q = p->next;

			p->next = NULL;
			p->prev = NULL;
			j = p->hash_key % tab_size;
			if (tab[j]) {
				tab[j]->prev = p;
				p->next = tab[j];
			}
			tab[j] = p;
		}
	}
	free(a_table->tab);
	a_table->tab = tab;
	a_table->tab_size = tab_size;

	return 0;
}

zc_hashtable_entry_t *zc_hashtable_get_entry(zc_hashtable_t * a_table, void *a_key)
{
	unsigned int i;
	zc_hashtable_entry_t *p;

	zc_assert(a_table, NULL);
	zc_assert(a_key, NULL);

	i = a_table->hash_fn(a_key) % a_table->tab_size;
	for (p = (a_table->tab)[i]; p; p = p->next) {
		if (a_table->equal_fn(a_key, p->key))
			return p;
	}

	return NULL;
}

void *zc_hashtable_get(zc_hashtable_t * a_table, void *a_key)
{
	zc_hashtable_entry_t *p;
	p = zc_hashtable_get_entry(a_table, a_key);

	return p ? p->value : NULL;
}

int zc_hashtable_put(zc_hashtable_t * a_table, void *a_key, void *a_value)
{
	int rc = 0;
	unsigned int i;
	zc_hashtable_entry_t *p = NULL;

	zc_assert(a_table, -1);
	zc_assert(a_key, -1);

	i = a_table->hash_fn(a_key) % a_table->tab_size;
	for (p = (a_table->tab)[i]; p; p = p->next) {
		if (a_table->equal_fn(a_key, p->key))
			break;
	}

	if (p) {
		if (a_table->key_del_fn) {
			a_table->key_del_fn(p->key);
		}
		if (a_table->value_del_fn) {
			a_table->value_del_fn(p->value);
		}
		p->key = a_key;
		p->value = a_value;
		return 0;
	} else {
		if (a_table->nelem > a_table->tab_size * 1.3) {
			rc = zc_hashtable_rehash(a_table);
			if (rc) {
				zc_error("rehash fail");
				return -1;
			}
		}

		p = calloc(1, sizeof(*p));
		if (!p) {
			zc_error("calloc fail, errno[%d]", errno);
			return -1;
		}

		p->hash_key = a_table->hash_fn(a_key);
		p->key = a_key;
		p->value = a_value;
		p->next = NULL;
		p->prev = NULL;

		i = p->hash_key % a_table->tab_size;
		if ((a_table->tab)[i]) {
			(a_table->tab)[i]->prev = p;
			p->next = (a_table->tab)[i];
		}
		(a_table->tab)[i] = p;
		a_table->nelem++;
	}

	return 0;
}

void zc_hashtable_remove(zc_hashtable_t * a_table, void *a_key)
{
	zc_hashtable_entry_t *p;
	unsigned int i;

	zc_assert(a_table,);
	zc_assert(a_key,);

	i = a_table->hash_fn(a_key) % a_table->tab_size;
	for (p = (a_table->tab)[i]; p; p = p->next) {
		if (a_table->equal_fn(a_key, p->key))
			break;
	}

	if (!p) {
		zc_error("p[%p] not found in hashtable");
		return;
	}

	if (a_table->key_del_fn) {
		a_table->key_del_fn(p->key);
	}
	if (a_table->value_del_fn) {
		a_table->value_del_fn(p->value);
	}

	if (p->next) {
		p->next->prev = p->prev;
	}
	if (p->prev) {
		p->prev->next = p->next;
	} else {
		unsigned int i;

		i = p->hash_key % a_table->tab_size;
		a_table->tab[i] = p->next;
	}

	free(p);
	a_table->nelem--;

	return;
}

zc_hashtable_entry_t *zc_hashtable_begin(zc_hashtable_t * a_table)
{
	size_t i;
	zc_hashtable_entry_t *p;

	zc_assert(a_table, NULL);

	for (i = 0; i < a_table->tab_size; i++) {
		for (p = (a_table->tab)[i]; p; p = p->next) {
			if (p)
				return p;
		}
	}

	return NULL;
}

zc_hashtable_entry_t *zc_hashtable_next(zc_hashtable_t * a_table, zc_hashtable_entry_t * a_entry)
{
	size_t i;
	size_t j;

	zc_assert(a_table, NULL);
	zc_assert(a_entry, NULL);

	if (a_entry->next)
		return a_entry->next;

	i = a_entry->hash_key % a_table->tab_size;

	for (j = i + 1; j < a_table->tab_size; j++) {
		if ((a_table->tab)[j]) {
			return (a_table->tab)[j];
		}
	}

	return NULL;
}

/*******************************************************************************/

unsigned int zc_hashtable_str_hash(void *str)
{
	unsigned int h = 0;
	const char *p = (const char *)str;

	while (*p != '\0')
		h = h * 129 + (unsigned int)(*p++);

	return h;
}

int zc_hashtable_str_equal(void *key1, void *key2)
{
	return (STRCMP((const char *)key1, ==, (const char *)key2));
}

unsigned int zc_hashtable_tid_hash(void *ptid)
{
	pthread_t tid;

	tid = *((pthread_t *) ptid);
	return (unsigned int)tid;
}

int zc_hashtable_tid_equal(void *ptid1, void *ptid2)
{
	pthread_t tid1;
	pthread_t tid2;

	tid1 = *((pthread_t *) ptid1);
	tid2 = *((pthread_t *) ptid2);

	return pthread_equal(tid1, tid2);
}