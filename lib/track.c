#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "sync.h"
#include "track.h"
#include "base.h"

static int key_linear(const struct track_key k[2], double row)
{
	double t = (row - k[0].row) / ((double)k[1].row - k[0].row);
	return (int)(k[0].value + ((double)k[1].value - k[0].value) * t);
}

static int key_short(const struct track_key k[2], double row)
{
	double t = (row - k[0].row) / ((double)k[1].row - k[0].row);

	int k0_top = k[0].value >> 16;
	int k0_bottom = k[0].value & 0xffff;
	int k1_top = k[1].value >> 16;
	int k1_bottom = k[1].value & 0xffff;

	int new_top = (int)(k0_top + ((double)k1_top - k0_top) * t);
	int new_bottom = (int)(k0_bottom + ((double)k1_bottom - k0_bottom) * t);

	return (new_bottom & 0xffff) | (new_top & 0xffff) << 16;
}

static int key_byte(const struct track_key k[2], double row)
{
	double t = (row - k[0].row) / ((double)k[1].row - k[0].row);

	int k0_b3 = k[0].value >> 24;
	int k0_b2 = (k[0].value >> 16) & 0xff;
	int k0_b1 = (k[0].value >> 8) & 0xff;
	int k0_b0 = k[0].value & 0xff;

	int k1_b3 = k[1].value >> 24;
	int k1_b2 = (k[1].value >> 16) & 0xff;
	int k1_b1 = (k[1].value >> 8) & 0xff;
	int k1_b0 = k[1].value & 0xff;

	int new_b3 = (int)(k0_b3 + ((double)k1_b3 - k0_b3) * t);
	int new_b2 = (int)(k0_b2 + ((double)k1_b2 - k0_b2) * t);
	int new_b1 = (int)(k0_b1 + ((double)k1_b1 - k0_b1) * t);
	int new_b0 = (int)(k0_b0 + ((double)k1_b0 - k0_b0) * t);

	return (new_b3 & 0xff) << 24 | (new_b2 & 0xff) << 16 | (new_b1 & 0xff) << 8 | (new_b0 & 0xff);
}

static int key_nibble(const struct track_key k[2], double row)
{
	double t = (row - k[0].row) / ((double)k[1].row - k[0].row);

	int k0_n7 = k[0].value >> 28;
	int k0_n6 = (k[0].value >> 24) & 0xf;
	int k0_n5 = (k[0].value >> 20) & 0xf; 
	int k0_n4 = (k[0].value >> 16) & 0xf;
	int k0_n3 = (k[0].value >> 12) & 0xf;
	int k0_n2 = (k[0].value >> 8) & 0xf;
	int k0_n1 = (k[0].value >> 4) & 0xf;
	int k0_n0 = k[0].value & 0xf;

	int k1_n7 = k[1].value >> 28;
	int k1_n6 = (k[1].value >> 24) & 0xf;
	int k1_n5 = (k[1].value >> 20) & 0xf;
	int k1_n4 = (k[1].value >> 16) & 0xf;
	int k1_n3 = (k[1].value >> 12) & 0xf;
	int k1_n2 = (k[1].value >> 8) & 0xf;
	int k1_n1 = (k[1].value >> 4) & 0xf;
	int k1_n0 = k[1].value & 0xf;

	int new_n7 = (int)(k0_n7 + ((double)k1_n7 - k0_n7) * t);
	int new_n6 = (int)(k0_n6 + ((double)k1_n6 - k0_n6) * t);
	int new_n5 = (int)(k0_n5 + ((double)k1_n5 - k0_n5) * t);
	int new_n4 = (int)(k0_n4 + ((double)k1_n4 - k0_n4) * t);
	int new_n3 = (int)(k0_n3 + ((double)k1_n3 - k0_n3) * t);
	int new_n2 = (int)(k0_n2 + ((double)k1_n2 - k0_n2) * t);
	int new_n1 = (int)(k0_n1 + ((double)k1_n1 - k0_n1) * t);
	int new_n0 = (int)(k0_n0 + ((double)k1_n0 - k0_n0) * t);

	return (new_n7 & 0xf) << 28 | (new_n6 & 0xf) << 24 | (new_n5 & 0xf) << 20 | (new_n4 & 0xf) << 16 | (new_n3 & 0xf) << 12 | (new_n2 & 0xf) << 8 | (new_n1 & 0xf) << 4 | (new_n0 & 0xf);
}

int sync_get_val(const struct sync_track *t, double row)
{
	int idx, irow;

	/* If we have no keys at all, return a constant 0 */
	if (!t->num_keys)
		return 0;

	irow = (int)floor(row);
	idx = key_idx_floor(t, irow);

	/* at the edges, return the first/last value */
	if (idx < 0)
		return t->keys[0].value;
	if (idx > (int)t->num_keys - 2)
		return t->keys[t->num_keys - 1].value;

	/* interpolate according to key-type */
	switch (t->keys[idx].type) {
	case KEY_STEP:
		return t->keys[idx].value;
	case KEY_LINEAR:
		return key_linear(t->keys + idx, row);
	case KEY_SHORT:
		return key_short(t->keys + idx, row);
	case KEY_BYTE:
		return key_byte(t->keys + idx, row);
	case KEY_NIBBLE:
		return key_nibble(t->keys + idx, row);
	default:
		assert(0);
		return 0;
	}
}

int sync_find_key(const struct sync_track *t, int row)
{
	int lo = 0, hi = t->num_keys;

	/* binary search, t->keys is sorted by row */
	while (lo < hi) {
		int mi = (lo + hi) / 2;
		assert(mi != hi);

		if (t->keys[mi].row < row)
			lo = mi + 1;
		else if (t->keys[mi].row > row)
			hi = mi;
		else
			return mi; /* exact hit */
	}
	assert(lo == hi);

	/* return first key after row, negated and biased (to allow -0) */
	return -lo - 1;
}

#ifndef SYNC_PLAYER
int sync_set_key(struct sync_track *t, const struct track_key *k)
{
	int idx = sync_find_key(t, k->row);
	if (idx < 0) {
		/* no exact hit, we need to allocate a new key */
		void *tmp;
		idx = -idx - 1;
		tmp = realloc(t->keys, sizeof(struct track_key) *
		    (t->num_keys + 1));
		if (!tmp)
			return -1;
		t->num_keys++;
		t->keys = tmp;
		memmove(t->keys + idx + 1, t->keys + idx,
		    sizeof(struct track_key) * (t->num_keys - idx - 1));
	}
	t->keys[idx] = *k;
	return 0;
}

int sync_del_key(struct sync_track *t, int pos)
{
	void *tmp;
	int idx = sync_find_key(t, pos);
	assert(idx >= 0);
	memmove(t->keys + idx, t->keys + idx + 1,
	    sizeof(struct track_key) * (t->num_keys - idx - 1));
	assert(t->keys);
	tmp = realloc(t->keys, sizeof(struct track_key) *
	    (t->num_keys - 1));
	if (t->num_keys != 1 && !tmp)
		return -1;
	t->num_keys--;
	t->keys = tmp;
	return 0;
}
#endif
