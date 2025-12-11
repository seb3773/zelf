// Copyright 1999-2015 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

An assert function which contains a breakpoint, for ease of debugging.

*/

#pragma once
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static inline void internal_error(void) {
	fflush(stdout);
	fprintf(stderr,
		"\n\nShrinkler has encountered an internal error.\n"
		"Please send a bug report to blueberry@loonies.dk,\n"
		"providing the file you tried to compress.\n"
		"\n"
		"Thanks, and apologies for the inconvenience.\n\n");
	exit(1);
}

#ifndef NDEBUG
static void _assert_func(const char *file, int line, const char *exp) {
	fflush(stdout);
	fprintf(stderr, "\n\nassertion \"%s\" failed: file \"%s\", line %d\n", exp, file, line);
	fflush(stderr);
#ifdef DEBUG
	__asm volatile ("int3;");
#endif
	internal_error();
}
#undef assert
#define assert(__e) ((__e) ? (void)0 : _assert_func (__FILE__, __LINE__,#__e))
#else
#ifndef assert
#define assert(__e) ((void)0)
#endif
#endif
