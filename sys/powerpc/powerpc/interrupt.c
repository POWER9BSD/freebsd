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
#include <machine/platform.h>

#include "pic_if.h"

#ifdef BOOKE
#define BOOKE_CLEAR_WE(framep) (framep)->srr1 &= ~PSL_WE
#else
#define BOOKE_CLEAR_WE(framep)
#endif

/*
 * A very short dispatch, to try and maximise assembler code use
 * between all exception types. Maybe 'true' interrupts should go
 * here, and the trap code can come in separately
 */
#ifdef __powerpc64__
static __inline register_t
save_context(struct thread *td, struct trapframe *newframe, bool softnmi)
{
	uint32_t flags;
	uint64_t msr;

	td->td_critnest++;
	td->td_intr_nesting_level++;
	td->td_intr_frame = newframe;
	flags = PCPU_GET(intr_flags);
	MPASS(flags & PPC_INTR_ENABLE);
	PCPU_SET(intr_flags, flags & ~PPC_INTR_ENABLE);
	if (softnmi) {
		msr = mfmsr();
		/*
		 * Soft disable interrupts before hard enabling
		 */
		mtmsr_ee(msr | PSL_EE);
	} else
		td->td_md.md_spinlock_count++;
	return (msr);
}

static __inline void
restore_context(struct thread *td, struct trapframe *oldframe, register_t msr,
			   bool softnmi)
{
	uint32_t flags;
	struct trapframe *framep;

	if (softnmi)
		mtmsr_ee(msr);
	framep = td->td_intr_frame;
	td->td_intr_nesting_level--;
	flags = PCPU_GET(intr_flags);
	if (softnmi) {
		MPASS((flags & PPC_INTR_ENABLE) == 0);
		if (PCPU_GET(intr_pend_flags) & PPC_PEND_MASK)
			delayed_interrupt(framep);
	} else
		td->td_md.md_spinlock_count--;
	PCPU_SET(intr_flags, flags | PPC_INTR_ENABLE);
	/*
	 * Clear soft disable of interrupts before return
	 */
	td->td_intr_frame = oldframe;
	td->td_critnest--;
}

void
delayed_interrupt(struct trapframe *framep)
{
	volatile uint8_t *pintr_pend_flags;
	int64_t pend_decr_sum, decrval;
	uint64_t msr;
	uint8_t ipflags;
	struct pcpu *pc;
	struct thread *td;

	td = curthread;
	pc = get_pcpu();
	MPASS((pc->pc_intr_flags & PPC_INTR_ENABLE) == 0);
	if (td->td_intr_nesting_level > 0)
		return;
	msr = mfmsr();
	td->td_intr_nesting_level++;
	pintr_pend_flags = &pc->pc_intr_pend_flags;
	while ((ipflags = *pintr_pend_flags) & PPC_PEND_MASK) {
		if (ipflags & PPC_DECR_PEND) {
			pend_decr_sum = pc->pc_pend_decr_sum;
			pc->pc_pend_decr_sum = 0;
			/*
			 * How many time base cycles have elapsed since
			 * the last decr trap occurred -
			 * the decrementer was loaded with INT_MAX
			 * so the delta is the number of time base
			 * increments
			 */
			__asm ("mfdec %0" : "=r"(decrval));
			decrval -= INT_MAX;
			pend_decr_sum += decrval;
			/*
			 * Reset the decrementer to the max
			 * value in case this turns out to be
			 * a no-op trap. The decr_intr handler
			 * will set decr to the proper divisor
			 * if we aren't actually running tickless.
			 */

			mtdec(INT_MAX);
			*pintr_pend_flags &= ~PPC_DECR_PEND;
			mtmsr_ee(msr | PSL_EE);
			decr_intr(framep, pend_decr_sum);
			mtmsr_ee(msr);
		}
	}

	td->td_intr_nesting_level--;
	MPASS((pc->pc_intr_flags & PPC_INTR_ENABLE) == 0);
	pc->pc_intr_flags |= PPC_INTR_ENABLE;
}

void
powerpc_interrupt(struct trapframe *framep)
{
	struct thread *td;
	struct trapframe *oldframe;
	register_t msr;
	uint64_t decrval;
	struct pcpu *pc;

	pc = get_pcpu();
	td = curthread;
	oldframe = td->td_intr_frame;

	CTR2(KTR_INTR, "%s: EXC=%x", __func__, framep->exc);
	/*
	 * Are interrupts soft disabled? -- check for NMI
	 */
	switch (framep->exc) {
	case EXC_EXI:
	case EXC_HVI:
		MPASS(pc->pc_intr_flags & PPC_INTR_ENABLE);
		msr = save_context(td, framep, false);
		PIC_DISPATCH(root_pic, framep);
		BOOKE_CLEAR_WE(framep);
		restore_context(td, oldframe, msr, false);
		break;

	case EXC_DECR:
		MPASS(pc->pc_intr_flags & PPC_INTR_ENABLE);
		decrval = pc->pc_pend_decr_sum;
		pc->pc_pend_decr_sum = 0;
		pc->pc_intr_pend_flags &= ~PPC_DECR_PEND;
		msr = save_context(td, framep, true);
		decr_intr(framep, decrval);
		BOOKE_CLEAR_WE(framep);
		restore_context(td, oldframe, msr, true);
		break;
#ifdef HWPMC_HOOKS
	case EXC_PERF:
		KASSERT(pmc_intr != NULL, ("Performance exception, but no handler!"));
		(*pmc_intr)(framep);
		if (pmc_hook && (PCPU_GET(curthread)->td_pflags & TDP_CALLCHAIN)) {
			msr = mfmsr();
			mtmsr_ee(msr | PSL_EE);
			KASSERT(framep->srr1 & PSL_EE,
				("TDP_CALLCHAIN set in interrupt disabled context"));
			pmc_hook(PCPU_GET(curthread), PMC_FN_USER_CALLCHAIN, framep);
			mtmsr_ee(msr);
		}
		break;
#endif

	default:
		/* Re-enable interrupts if applicable. */
		if (framep->srr1 & PSL_EE)
			mtmsr_ee(mfmsr() | PSL_EE);
		trap(framep);
	}
}

#else
void
powerpc_interrupt(struct trapframe *framep)
{
	struct thread *td;
	struct trapframe *oldframe;
	register_t ee;
	td = curthread;
	CTR2(KTR_INTR, "%s: EXC=%x", __func__, framep->exc);

	switch (framep->exc) {
	case EXC_EXI:
	case EXC_HVI:
		critical_enter();
		PIC_DISPATCH(root_pic, framep);
		critical_exit();
		BOOKE_CLEAR_WE(framep);
		break;

	case EXC_DECR: {
		critical_enter();
		atomic_add_int(&td->td_intr_nesting_level, 1);
		oldframe = td->td_intr_frame;
		td->td_intr_frame = framep;
		decr_intr(framep);
		td->td_intr_frame = oldframe;
		atomic_subtract_int(&td->td_intr_nesting_level, 1);
		critical_exit();
		BOOKE_CLEAR_WE(framep);
	}
		break;
#ifdef HWPMC_HOOKS
	case EXC_PERF:
		critical_enter();
		KASSERT(pmc_intr != NULL, ("Performance exception, but no handler!"));
		(*pmc_intr)(framep);
		if (pmc_hook && (PCPU_GET(curthread)->td_pflags & TDP_CALLCHAIN))
			pmc_hook(PCPU_GET(curthread), PMC_FN_USER_CALLCHAIN, framep);
		critical_exit();
		break;
#endif

	default:
		/* Re-enable interrupts if applicable. */
		ee = framep->srr1 & PSL_EE;
		if (ee != 0)
			mtmsr(mfmsr() | ee);
		trap(framep);
	}	        
}
#endif
