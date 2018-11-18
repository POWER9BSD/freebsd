/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2018 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ASM_ATOMIC_H_
#define	_ASM_ATOMIC_H_

#include <linux/compiler.h>
#include <sys/types.h>
#include <machine/atomic.h>

#define	ATOMIC_INIT(x)	{ .counter = (x) }

typedef struct {
	volatile int counter;
} atomic_t;

static inline void
atomic_set_release(atomic_t *v, int i)
{
	atomic_store_rel_int(&v->counter, i);
}

static inline void
atomic_set_mask(unsigned int mask, atomic_t *v)
{
	atomic_set_int(&v->counter, mask);
}

static inline int
atomic_inc(atomic_t *v)
{
	return atomic_fetchadd_int(&v->counter, 1) + 1;
}

static inline int
atomic_dec(atomic_t *v)
{
	return atomic_fetchadd_int(&v->counter, -1) - 1;
}

static inline void
atomic_clear_mask(unsigned int mask, atomic_t *v)
{
	atomic_clear_int(&v->counter, mask);
}

#ifdef __powerpc__
#include <powerpc/atomic.h>
#else
#include <x86/atomic.h>
#endif


#define __atomic_op_fence(op, args...)					\
({									\
	typeof(op##_relaxed(args)) __ret;				\
	mb();											\
	__ret = op##_relaxed(args);						\
	mb();											\
	__ret;								\
})

#define atomic_xchg(ptr, v)		(xchg(&(ptr)->counter, (v)))
#define atomic_cmpxchg(v, old, new)	(cmpxchg(&((v)->counter), (old), (new)))

#ifndef atomic_andnot
#define atomic_andnot(i, v)		atomic_and(~(int)(i), (v))
#endif

#ifndef atomic_add_return
#define  atomic_add_return(...)						\
	__atomic_op_fence(atomic_add_return, __VA_ARGS__)
#endif

#ifndef atomic_sub_return
#define  atomic_sub_return(...)						\
	__atomic_op_fence(atomic_sub_return, __VA_ARGS__)
#endif

#ifndef atomic_fetch_or
#define atomic_fetch_or(...)						\
	__atomic_op_fence(atomic_fetch_or, __VA_ARGS__)
#endif

#ifndef atomic_fetch_and
#define atomic_fetch_and(...)						\
	__atomic_op_fence(atomic_fetch_and, __VA_ARGS__)
#endif

#ifndef atomic_fetch_andnot
#define atomic_fetch_andnot(i, v)		atomic_fetch_and(~(int)(i), (v))
#endif

/*------------------------------------------------------------------------*
 *	32-bit atomic operations
 *------------------------------------------------------------------------*/

#define	atomic_add(i, v)		atomic_add_return((i), (v))
#define	atomic_sub(i, v)		atomic_sub_return((i), (v))
#define	atomic_inc_return(v)		atomic_add_return(1, (v))
#define	atomic_add_negative(i, v)	(atomic_add_return((i), (v)) < 0)
#define	atomic_add_and_test(i, v)	(atomic_add_return((i), (v)) == 0)
#define	atomic_sub_and_test(i, v)	(atomic_sub_return((i), (v)) == 0)
#define	atomic_dec_and_test(v)		(atomic_sub_return(1, (v)) == 0)
#define	atomic_inc_and_test(v)		(atomic_add_return(1, (v)) == 0)
#define	atomic_dec_return(v)		atomic_sub_return(1, (v))
#define	atomic_add_unless(v, a, u)	atomic_fetch_add_unless((v), (a), (u))
#define	atomic_inc_not_zero(v)		atomic_add_unless((v), 1, 0)

#endif					/* _ASM_ATOMIC_H_ */
