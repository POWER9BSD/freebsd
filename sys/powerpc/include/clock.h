/*-
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL

struct trapframe;
#ifdef __powerpc64__
void	decr_intr(struct trapframe *, int64_t);
#else
void	decr_intr(struct trapframe *);
#endif

#endif

#endif /* !_MACHINE_CLOCK_H_ */
