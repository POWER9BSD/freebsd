/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018, Matthew Macy <mmacy@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef __POWERPC_INTERRUPT_H_
#define __POWERPC_INTERRUPT_H_

#ifdef __powerpc64__
#include <sys/systm.h>
#include <sys/pcpu.h>
#include <machine/trap.h>
extern void delayed_interrupt(void);

static __inline register_t
intr_disable(void)
{
	register_t msr;
	int intr_flags;
	struct pcpu *pcpupp;

	pcpupp = get_pcpu();
	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);
	intr_flags = pcpupp->pc_intr_flags;
	pcpupp->pc_intr_flags |= PPC_INTR_DISABLE;
	
	mtmsr(msr);
	return (intr_flags);
}

static __inline void
intr_restore(register_t flags)
{
	register_t msr;
	struct pcpu *pcpupp;

	if (flags & PPC_INTR_DISABLE)
		return;
	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);
	pcpupp = get_pcpu();
	if (__predict_false(pcpupp->pc_intr_flags & PPC_INTR_PEND)) {
		delayed_interrupt();
	}
	pcpupp->pc_intr_flags &= ~PPC_INTR_DISABLE;
	mtmsr(msr);
}
#else

static __inline register_t
intr_disable(void)
{
	register_t msr;

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);
	return (msr);
}

static __inline void
intr_restore(register_t msr)
{
	mtmsr(msr);
}
#endif /* !__powerpc64__ */
#endif /*  __POWERPC_INTERRUPT_H_ */
