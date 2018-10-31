/*-
 * Copyright 2002 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Interrupts are dispatched to here from locore asm
 */

#include "opt_hwpmc_hooks.h"

#include <sys/cdefs.h>                  /* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
#endif
#include <sys/proc.h>
#include <sys/kdb.h>
#include <sys/smp.h>
#include <sys/unistd.h>
#include <sys/vmmeter.h>

#include <machine/cpu.h>
#include <machine/clock.h>
#include <machine/db_machdep.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/spr.h>
#include <machine/sr.h>

#include "pic_if.h"

static register_t
save_context(struct thread *td, struct trapframe *newframe)
{
	uint32_t flags;
	uint64_t msr;

	td->td_critnest++;
	td->td_intr_nesting_level++;
	flags = PCPU_GET(intr_flags);
	MPASS((flags & PPC_INTR_DISABLE) == 0);
	td->td_intr_frame = newframe;
	msr = mfmsr();
#ifdef __powerpc64__
	/*
	 * Soft disable interrupts before hard enabling
	 */
	PCPU_SET(intr_flags, flags | PPC_INTR_DISABLE);
	mtmsr(msr | PSL_EE);
#endif
	return (msr);
}

static void
restore_context(struct thread *td, struct trapframe *oldframe, register_t msr)
{
	uint32_t flags __unused;

#ifdef __powerpc64__
	mtmsr(msr);
	flags = PCPU_GET(intr_flags);
	MPASS(flags & PPC_INTR_DISABLE);

	/*
	 * Clear soft disable of interrupts before return
	 */
	PCPU_SET(intr_flags, flags & ~PPC_INTR_DISABLE);
#endif
	td->td_intr_frame = oldframe;
	td->td_intr_nesting_level--;
	td->td_critnest--;
}

#ifdef __powerpc64__
void
delayed_interrupt(struct trapframe *framep)
{
	uint32_t pend_decr, *intr_flags;
	int32_t decrval, tmp;
	uint64_t msr;
	struct pcpu *pcpupp;
	struct thread *td;

	pcpupp = get_pcpu();
	td = curthread;
	msr = mfmsr();
	td->td_intr_nesting_level++;
	intr_flags = &pcpupp->pc_intr_flags;
	while (*intr_flags & PPC_PEND_MASK) {
#ifdef notyet
		uint32_t pend_exi, pend_hvi;
		pend_exi = pcpupp->pc_pend_exi;
		pend_hvi = pcpupp->pc_pend_hvi;
#endif
		pcpupp->pc_pend_exi = 0;
		pcpupp->pc_pend_hvi = 0;
		/*
		 * Update persistent pend stats XXX
		 */
		if (*intr_flags & PPC_DECR_PEND) {
			pend_decr = pcpupp->pc_pend_decr;
			pcpupp->pc_pend_decr = 0;
			/*
			 * Reset decrementer before enabling
			 * interrupts
			 */
			tmp = 0x7fffffff;
			__asm ("mfdec %0" : "=r"(decrval));
			__asm ("mtdec %0" : "=r"(tmp));

			*intr_flags &= ~PPC_DECR_PEND;
			mtmsr(msr | PSL_EE);
			decr_intr(framep, pend_decr, decrval);
			mtmsr(msr);
		}
		if (*intr_flags & (PPC_EXI_PEND|PPC_HVI_PEND)) {
			*intr_flags &= ~(PPC_EXI_PEND|PPC_HVI_PEND);
			mtmsr(msr | PSL_EE);

			PIC_DISPATCH(root_pic, framep);
			mtmsr(msr);
		}
	}
	td->td_intr_nesting_level--;
}
#endif

/*
 * A very short dispatch, to try and maximise assembler code use
 * between all exception types. Maybe 'true' interrupts should go
 * here, and the trap code can come in separately
 */
void
powerpc_interrupt(struct trapframe *framep)
{
	struct thread *td;
	struct trapframe *oldframe;
	register_t msr;
	uint32_t pend_decr, tmp, decrval;
	struct pcpu *pcpupp;

	pcpupp = get_pcpu();
	td = curthread;
	oldframe = td->td_intr_frame;
	CTR2(KTR_INTR, "%s: EXC=%x", __func__, framep->exc);
	/*
	 * We only leave interrupts disabled for PMC
	 */
	switch (framep->exc) {
	case EXC_EXI:
		pcpupp->pc_pend_exi = 0;
		pcpupp->pc_intr_flags &= PPC_EXI_PEND;
		break;
	case EXC_HVI:
		pcpupp->pc_pend_hvi = 0;
		pcpupp->pc_intr_flags &= PPC_HVI_PEND;
		break;
	}
	switch (framep->exc) {
	case EXC_EXI:
	case EXC_HVI:
		msr = save_context(td, framep);
		PIC_DISPATCH(root_pic, framep);
#ifdef BOOKE
		framep->srr1 &= ~PSL_WE;
#endif
		goto check_missed;
		break;

	case EXC_DECR:
		pend_decr = pcpupp->pc_pend_decr;
		pcpupp->pc_pend_decr = 0;
		pcpupp->pc_intr_flags &= ~PPC_DECR_PEND;
		/*
		 * Reset decrementer before enabling
		 * interrupts
		 */
		tmp = 0x7fffffff;
		__asm ("mfdec %0" : "=r"(decrval));
		__asm ("mtdec %0" : "=r"(tmp));
		msr = save_context(td, framep);
		decr_intr(framep, pend_decr, decrval);
		goto check_missed;
#ifdef BOOKE
		framep->srr1 &= ~PSL_WE;
#endif
		break;
#ifdef HWPMC_HOOKS
	case EXC_PERF:
		KASSERT(pmc_intr != NULL, ("Performance exception, but no handler!"));
		(*pmc_intr)(framep);
		if (pmc_hook && (PCPU_GET(curthread)->td_pflags & TDP_CALLCHAIN)) {
			msr = mfmsr();
#ifdef __powerpc64__
			mtmsr(msr | PSL_EE);
			KASSERT(framep->srr1 & PSL_EE,
				("TDP_CALLCHAIN set in interrupt disabled context"));
#endif
			pmc_hook(PCPU_GET(curthread), PMC_FN_USER_CALLCHAIN, framep);
			mtmsr(msr);
		}
		break;
#endif

	default:
		/* Re-enable interrupts if applicable. */
		if (framep->srr1 & PSL_EE)
			mtmsr(mfmsr() | PSL_EE);
		if (__predict_false(pcpupp->pc_intr_flags & PPC_INTR_DISABLE))
			td->td_critnest++;
		trap(framep);
		if (__predict_false(pcpupp->pc_intr_flags & PPC_INTR_DISABLE))
			td->td_critnest--;
	}
	return;
 check_missed:
	if (PCPU_GET(intr_flags) & PPC_PEND_MASK)
		delayed_interrupt(framep);
	restore_context(td, oldframe, msr);
}
