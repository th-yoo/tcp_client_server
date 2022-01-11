#include <stdio.h>

typedef struct repeat_s {
	int (*proc)(struct repeat_s* r, ...);
	int try_again;
} repeat_t;

int repeat(repeat_t* r, ...)
{
	int rv;
	do {
		rv = r->proc(r);
	} while (r->try_again);

	return rv;
}

struct count10 {
	repeat_t r;
	int cur;
};

int count10_proc(repeat_t* r)
{
	struct count10* self = (struct count10*)r;

	printf("cur: %d\n", self->cur);

	if (++self->cur == 10) {
		r->try_again = 0;
	}

	return 0;
}

struct count10 count = {
	{ count10_proc
	, 1 },
	0
};

int main()
{
	repeat(&count);
	return 0;
}
