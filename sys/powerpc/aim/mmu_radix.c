/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Matthew Macy
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/endian.h>
#include <sys/kerneldump.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/syslog.h>
#include <sys/msgbuf.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>
#include <sys/smp.h>

#include <sys/kdb.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>

#include <machine/_inttypes.h>
#include <machine/cpu.h>
#include <machine/platform.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/bat.h>
#include <machine/hid.h>
#include <machine/pte.h>
#include <machine/sr.h>
#include <machine/trap.h>
#include <machine/mmuvar.h>

#ifndef MMU_DIRECT
#include "mmu_oea64.h"
#include "mmu_if.h"
#include "moea64_if.h"

/*
 * Kernel MMU interface
 */
static void mmu_radix_advise(mmu_t, pmap_t, vm_offset_t, vm_offset_t, int);
static void mmu_radix_align_superpage(mmu_t, vm_object_t, vm_ooffset_t,
	vm_offset_t *, vm_size_t);
	
static void	mmu_radix_bootstrap(mmu_t mmup, 
		    vm_offset_t kernelstart, vm_offset_t kernelend);
static void mmu_radix_clear_modify(mmu_t, vm_page_t);
static int mmu_radix_change_attr(mmu_t, vm_offset_t, vm_size_t, vm_memattr_t);

static void mmu_radix_copy(mmu_t, pmap_t, pmap_t, vm_offset_t,
    vm_size_t, vm_offset_t);
static void mmu_radix_copy_page(mmu_t, vm_page_t, vm_page_t);
static void mmu_radix_copy_pages(mmu_t mmu, vm_page_t *ma, vm_offset_t a_offset,
    vm_page_t *mb, vm_offset_t b_offset, int xfersize);
static int mmu_radix_enter(mmu_t, pmap_t, vm_offset_t, vm_page_t, vm_prot_t,
    u_int flags, int8_t psind);
static void mmu_radix_enter_object(mmu_t, pmap_t, vm_offset_t, vm_offset_t, vm_page_t,
    vm_prot_t);
static void mmu_radix_enter_quick(mmu_t, pmap_t, vm_offset_t, vm_page_t, vm_prot_t);
static vm_paddr_t mmu_radix_extract(mmu_t, pmap_t, vm_offset_t);
static vm_page_t mmu_radix_extract_and_hold(mmu_t, pmap_t, vm_offset_t, vm_prot_t);
static void mmu_radix_growkernel(mmu_t, vm_offset_t);
static void mmu_radix_init(mmu_t);
static int mmu_radix_mincore(mmu_t, pmap_t, vm_offset_t, vm_paddr_t *);
static boolean_t mmu_radix_is_modified(mmu_t, vm_page_t);
static boolean_t mmu_radix_is_prefaultable(mmu_t, pmap_t, vm_offset_t);
static boolean_t mmu_radix_is_referenced(mmu_t, vm_page_t);
static void mmu_radix_kremove(mmu_t mmup, vm_offset_t va);
static int mmu_radix_ts_referenced(mmu_t, vm_page_t);
static vm_offset_t mmu_radix_map(mmu_t, vm_offset_t *, vm_paddr_t, vm_paddr_t, int);
static boolean_t mmu_radix_page_exists_quick(mmu_t, pmap_t, vm_page_t);
static void mmu_radix_page_init(mmu_t, vm_page_t);
static void mmu_radix_object_init_pt(mmu_t, pmap_t, vm_offset_t, vm_object_t,
	vm_pindex_t, vm_size_t);
static int mmu_radix_page_wired_mappings(mmu_t, vm_page_t);
static void mmu_radix_pinit(mmu_t, pmap_t);
static void mmu_radix_pinit0(mmu_t, pmap_t);
static void mmu_radix_protect(mmu_t, pmap_t, vm_offset_t, vm_offset_t, vm_prot_t);
static void mmu_radix_qenter(mmu_t, vm_offset_t, vm_page_t *, int);
static void mmu_radix_qremove(mmu_t, vm_offset_t, int);
static void mmu_radix_release(mmu_t, pmap_t);
static void mmu_radix_remove(mmu_t, pmap_t, vm_offset_t, vm_offset_t);
static void mmu_radix_remove_pages(mmu_t, pmap_t);
static void mmu_radix_remove_all(mmu_t, vm_page_t);
static void mmu_radix_remove_write(mmu_t, vm_page_t);
static void mmu_radix_unwire(mmu_t, pmap_t, vm_offset_t, vm_offset_t);
static void mmu_radix_zero_page(mmu_t, vm_page_t);
static void mmu_radix_zero_page_area(mmu_t, vm_page_t, int, int);
static void mmu_radix_activate(mmu_t, struct thread *);
static void mmu_radix_deactivate(mmu_t, struct thread *);
static void *mmu_radix_mapdev(mmu_t, vm_paddr_t, vm_size_t);
static void *mmu_radix_mapdev_attr(mmu_t, vm_paddr_t, vm_size_t, vm_memattr_t);
static void mmu_radix_unmapdev(mmu_t, vm_offset_t, vm_size_t);
static vm_paddr_t mmu_radix_kextract(mmu_t, vm_offset_t);
static void mmu_radix_page_set_memattr(mmu_t, vm_page_t m, vm_memattr_t ma);
static void mmu_radix_kenter_attr(mmu_t, vm_offset_t, vm_paddr_t, vm_memattr_t ma);
static void mmu_radix_kenter(mmu_t, vm_offset_t, vm_paddr_t);
static boolean_t mmu_radix_dev_direct_mapped(mmu_t, vm_paddr_t, vm_size_t);
static void mmu_radix_sync_icache(mmu_t, pmap_t, vm_offset_t, vm_size_t);
static void mmu_radix_dumpsys_map(mmu_t mmu, vm_paddr_t pa, size_t sz,
    void **va);
static void mmu_radix_scan_init(mmu_t mmu);
static vm_offset_t mmu_radix_quick_enter_page(mmu_t mmu, vm_page_t m);
static void mmu_radix_quick_remove_page(mmu_t mmu, vm_offset_t addr);
static int mmu_radix_map_user_ptr(mmu_t mmu, pmap_t pm,
    volatile const void *uaddr, void **kaddr, size_t ulen, size_t *klen);
static int mmu_radix_decode_kernel_ptr(mmu_t mmu, vm_offset_t addr,
    int *is_user, vm_offset_t *decoded_addr);

static mmu_method_t mmu_radix_methods[] = {
	MMUMETHOD(mmu_activate,		mmu_radix_activate),
	MMUMETHOD(mmu_advise,	mmu_radix_advise),
	MMUMETHOD(mmu_align_superpage,	mmu_radix_align_superpage),
	MMUMETHOD(mmu_bootstrap,	mmu_radix_bootstrap),
	MMUMETHOD(mmu_change_attr,	mmu_radix_change_attr),
	MMUMETHOD(mmu_clear_modify,	mmu_radix_clear_modify),
	MMUMETHOD(mmu_copy,	mmu_radix_copy),
	MMUMETHOD(mmu_copy_page,	mmu_radix_copy_page),
	MMUMETHOD(mmu_copy_pages,	mmu_radix_copy_pages),
	MMUMETHOD(mmu_deactivate,      	mmu_radix_deactivate),
	MMUMETHOD(mmu_enter,		mmu_radix_enter),
	MMUMETHOD(mmu_enter_object,	mmu_radix_enter_object),
	MMUMETHOD(mmu_enter_quick,	mmu_radix_enter_quick),
	MMUMETHOD(mmu_extract,		mmu_radix_extract),
	MMUMETHOD(mmu_extract_and_hold,	mmu_radix_extract_and_hold),
	MMUMETHOD(mmu_growkernel,		mmu_radix_growkernel),
	MMUMETHOD(mmu_init,		mmu_radix_init),
	MMUMETHOD(mmu_is_modified,	mmu_radix_is_modified),
	MMUMETHOD(mmu_is_prefaultable,	mmu_radix_is_prefaultable),
	MMUMETHOD(mmu_is_referenced,	mmu_radix_is_referenced),
	MMUMETHOD(mmu_kremove,	mmu_radix_kremove),
	MMUMETHOD(mmu_map,     		mmu_radix_map),
	MMUMETHOD(mmu_mincore,     		mmu_radix_mincore),
	MMUMETHOD(mmu_object_init_pt,     		mmu_radix_object_init_pt),
	MMUMETHOD(mmu_page_exists_quick,mmu_radix_page_exists_quick),
	MMUMETHOD(mmu_page_init,	mmu_radix_page_init),
	MMUMETHOD(mmu_page_wired_mappings,mmu_radix_page_wired_mappings),
	MMUMETHOD(mmu_pinit,		mmu_radix_pinit),
	MMUMETHOD(mmu_pinit0,		mmu_radix_pinit0),
	MMUMETHOD(mmu_protect,		mmu_radix_protect),
	MMUMETHOD(mmu_qenter,		mmu_radix_qenter),
	MMUMETHOD(mmu_qremove,		mmu_radix_qremove),
	MMUMETHOD(mmu_release,		mmu_radix_release),
	MMUMETHOD(mmu_remove,		mmu_radix_remove),
	MMUMETHOD(mmu_remove_pages,	mmu_radix_remove_pages),
	MMUMETHOD(mmu_remove_all,      	mmu_radix_remove_all),
	MMUMETHOD(mmu_remove_write,	mmu_radix_remove_write),
	MMUMETHOD(mmu_sync_icache,	mmu_radix_sync_icache),
	MMUMETHOD(mmu_ts_referenced,	mmu_radix_ts_referenced),
	MMUMETHOD(mmu_unwire,		mmu_radix_unwire),
	MMUMETHOD(mmu_zero_page,       	mmu_radix_zero_page),
	MMUMETHOD(mmu_zero_page_area,	mmu_radix_zero_page_area),
	MMUMETHOD(mmu_page_set_memattr,	mmu_radix_page_set_memattr),
	MMUMETHOD(mmu_quick_enter_page, mmu_radix_quick_enter_page),
	MMUMETHOD(mmu_quick_remove_page, mmu_radix_quick_remove_page),

	MMUMETHOD(mmu_mapdev,		mmu_radix_mapdev),
	MMUMETHOD(mmu_mapdev_attr,	mmu_radix_mapdev_attr),
	MMUMETHOD(mmu_unmapdev,		mmu_radix_unmapdev),
	MMUMETHOD(mmu_kextract,		mmu_radix_kextract),
	MMUMETHOD(mmu_kenter,		mmu_radix_kenter),
	MMUMETHOD(mmu_kenter_attr,	mmu_radix_kenter_attr),
	MMUMETHOD(mmu_dev_direct_mapped,mmu_radix_dev_direct_mapped),
	MMUMETHOD(mmu_scan_init,	mmu_radix_scan_init),
	MMUMETHOD(mmu_dumpsys_map,	mmu_radix_dumpsys_map),
	MMUMETHOD(mmu_map_user_ptr,	mmu_radix_map_user_ptr),
	MMUMETHOD(mmu_decode_kernel_ptr, mmu_radix_decode_kernel_ptr),
	{ 0, 0 }
};

MMU_DEF(mmu_radix, MMU_TYPE_RADIX, mmu_radix_methods, 0);

#define METHOD(m) mmu_radix_ ## m(mmu_t mmup, 
#define METHODVOID(m) mmu_radix_ ## m(mmu_t mmup)
#define DUMPMETHOD(m) mmu_radix_dumpsys_ ## m(mmu_t mmup,
#define DUMPMETHODVOID(m) mmu_radix_dumpsys_ ## m(mmu_t mmup)
#define VISIBILITY static
#else
#define METHOD(m) pmap_ ## m(
#define METHODVOID(m) pmap_ ## m(void)
#define DUMPMETHOD(m) dumpsys_ ## m(
#define DUMPMETHODVOID(m) dumpsys_ ## m(void)
#define VISIBILITY

struct pmap kernel_pmap_store;

#endif

#define UNIMPLEMENTED() panic("%s not implemented", __func__)

#define RIC_FLUSH_TLB 0
#define RIC_FLUSH_PWC 1
#define RIC_FLUSH_ALL 2

#define POWER9_TLB_SETS_RADIX	128	/* # sets in POWER9 TLB Radix mode */

#define PPC_INST_TLBIE			0x7c000264
#define PPC_INST_TLBIEL			0x7c000224
#define PPC_INST_SLBIA			0x7c0003e4

#define TLBIEL_INVAL_SEL_MASK	0xc00	/* invalidation selector */
#define  TLBIEL_INVAL_PAGE	0x000	/* invalidate a single page */
#define  TLBIEL_INVAL_SET_LPID	0x800	/* invalidate a set for current LPID */
#define  TLBIEL_INVAL_SET	0xc00	/* invalidate a set for all LPIDs */

#define ___PPC_RA(a)	(((a) & 0x1f) << 16)
#define ___PPC_RB(b)	(((b) & 0x1f) << 11)
#define ___PPC_RS(s)	(((s) & 0x1f) << 21)
#define ___PPC_RT(t)	___PPC_RS(t)
#define ___PPC_R(r)	(((r) & 0x1) << 16)
#define ___PPC_PRS(prs)	(((prs) & 0x1) << 17)
#define ___PPC_RIC(ric)	(((ric) & 0x3) << 18)

#define PPC_SLBIA(IH)	__XSTRING(.long PPC_INST_SLBIA | \
				       ((IH & 0x7) << 21))
#define	PPC_TLBIE_5(rb,rs,ric,prs,r)				\
	__XSTRING(.long PPC_INST_TLBIE |								\
			  ___PPC_RB(rb) | ___PPC_RS(rs) |						\
			  ___PPC_RIC(ric) | ___PPC_PRS(prs) |					\
			  ___PPC_R(r))

#define	PPC_TLBIEL(rb,rs,ric,prs,r) \
	 __XSTRING(.long PPC_INST_TLBIEL | \
			   ___PPC_RB(rb) | ___PPC_RS(rs) |			\
			   ___PPC_RIC(ric) | ___PPC_PRS(prs) |		\
			   ___PPC_R(r))

#define PPC_INVALIDATE_ERAT		PPC_SLBIA(7)

/* Number of supported PID bits */
static unsigned int isa3_pid_bits;

/* PID to start allocating from */
static unsigned int isa3_base_pid;

#define PROCTAB_SIZE_SHIFT	(isa3_pid_bits + 4)
#define PROCTAB_ENTRIES	(1ul << isa3_pid_bits)


/*
 * Map of physical memory regions.
 */
static struct	mem_region *regions;
static struct	mem_region *pregions;
static u_int	phys_avail_count;
static int	regions_sz, pregions_sz;
static struct pate *isa3_parttab;
static struct prte *isa3_proctab;

#define	RADIX_PGD_SIZE_SHIFT	16
#define RADIX_PGD_SIZE	(1UL << RADIX_PGD_SIZE_SHIFT)

#define	RADIX_PGD_INDEX_SHIFT	(RADIX_PGD_SIZE_SHIFT-3)

/* POWER9 only permits a 64k partition table size. */
#define	PARTTAB_SIZE_SHIFT	16
#define PARTTAB_SIZE	(1UL << PARTTAB_SIZE_SHIFT)

#define PARTTAB_HR		(1UL << 63) /* host uses radix */
#define PARTTAB_GR		(1UL << 63) /* guest uses radix must match host */

/* TLB flush actions. Used as argument to tlbiel_all() */
enum {
	TLB_INVAL_SCOPE_LPID = 0,	/* invalidate TLBs for current LPID */
	TLB_INVAL_SCOPE_GLOBAL = 1,	/* invalidate all TLBs */
};

/*
 * We support 52 bits, hence:
 * bits 52 - 31 = 21, 0b10101
 * RTS encoding details
 * bits 0 - 3 of rts -> bits 6 - 8 unsigned long
 * bits 4 - 5 of rts -> bits 62 - 63 of unsigned long
 */
#define RTS_SIZE ((0x2UL << 61) | (0x5UL << 5))


static int powernv_enabled = 1;


static inline void
tlbiel_radix_set_isa300(uint32_t set, uint32_t is,
	uint32_t pid, uint32_t ric, uint32_t prs)
{
	uint64_t rb;
	uint64_t rs;

	rb = PPC_BITLSHIFT_VAL(set, 51) | PPC_BITLSHIFT_VAL(is, 53);
	rs = PPC_BITLSHIFT_VAL((uint64_t)pid, 31);

	__asm __volatile(PPC_TLBIEL(%0, %1, %2, %3, 1)
		     : : "r"(rb), "r"(rs), "i"(ric), "i"(prs)
		     : "memory");
}

static void
tlbiel_flush_isa3(uint32_t num_sets, uint32_t is)
{
	uint32_t set;

	__asm __volatile("ptesync": : :"memory");

	/*
	 * Flush the first set of the TLB, and the entire Page Walk Cache
	 * and partition table entries. Then flush the remaining sets of the
	 * TLB.
	 */
	tlbiel_radix_set_isa300(0, is, 0, RIC_FLUSH_ALL, 0);
	for (set = 1; set < num_sets; set++)
		tlbiel_radix_set_isa300(set, is, 0, RIC_FLUSH_TLB, 0);

	/* Do the same for process scoped entries. */
	tlbiel_radix_set_isa300(0, is, 0, RIC_FLUSH_ALL, 1);
	for (set = 1; set < num_sets; set++)
		tlbiel_radix_set_isa300(set, is, 0, RIC_FLUSH_TLB, 1);

	__asm __volatile("ptesync": : :"memory");
}

static void
mmu_radix_tlbiel_flush(int scope)
{
	int is;

	MPASS(scope == TLB_INVAL_SCOPE_LPID ||
		  scope == TLB_INVAL_SCOPE_GLOBAL);
	is = scope + 2;

	tlbiel_flush_isa3(POWER9_TLB_SETS_RADIX, is);
	__asm __volatile(PPC_INVALIDATE_ERAT "; isync" : : :"memory");
}

static void
mmu_radix_init_amor(void)
{
	/*
	* In HV mode, we init AMOR (Authority Mask Override Register) so that
	* the hypervisor and guest can setup IAMR (Instruction Authority Mask
	* Register), enable key 0 and set it to 1.
	*
	* AMOR = 0b1100 .... 0000 (Mask for key 0 is 11)
	*/
	mtspr(SPR_AMOR, (3ul << 62));
}

static void
mmu_radix_init_iamr(void)
{
	/*
	 * Radix always uses key0 of the IAMR to determine if an access is
	 * allowed. We set bit 0 (IBM bit 1) of key0, to prevent instruction
	 * fetch.
	 */
	mtspr(SPR_IAMR, (1ul << 62));
}

static void
mmu_radix_pid_set(pmap_t pmap)
{
	mtspr(SPR_PID, pmap->pm_pid);
	isync();
}

/* Quick sort callout for comparing physical addresses. */
static int
pa_cmp(const void *a, const void *b)
{
	const vm_paddr_t *pa = a, *pb = b;

	if (*pa < *pb)
		return (-1);
	else if (*pa > *pb)
		return (1);
	else
		return (0);
}

static void
mmu_radix_early_bootstrap(vm_offset_t start, vm_offset_t end)
{
	vm_paddr_t	kpstart, kpend, l1phys;
	vm_size_t	physsz, hwphyssz;
	//uint64_t	l2virt;
	int		rm_pavail;
	int		i, j;

	kpstart = start & ~DMAP_BASE_ADDRESS;
	kpend = end & ~DMAP_BASE_ADDRESS;

	/* Get physical memory regions from firmware */
	mem_regions(&pregions, &pregions_sz, &regions, &regions_sz);
	CTR0(KTR_PMAP, "mmu_radix_early_bootstrap: physical memory");

	if (sizeof(phys_avail)/sizeof(phys_avail[0]) < regions_sz)
		panic("mmu_radix_early_bootstrap: phys_avail too small");

	phys_avail_count = 0;
	physsz = 0;
	hwphyssz = 0;
	TUNABLE_ULONG_FETCH("hw.physmem", (u_long *) &hwphyssz);
	for (i = 0, j = 0; i < regions_sz; i++, j += 2) {
		CTR3(KTR_PMAP, "region: %#zx - %#zx (%#zx)",
		    regions[i].mr_start, regions[i].mr_start +
		    regions[i].mr_size, regions[i].mr_size);
		if (hwphyssz != 0 &&
		    (physsz + regions[i].mr_size) >= hwphyssz) {
			if (physsz < hwphyssz) {
				phys_avail[j] = regions[i].mr_start;
				phys_avail[j + 1] = regions[i].mr_start +
				    hwphyssz - physsz;
				physsz = hwphyssz;
				phys_avail_count++;
			}
			break;
		}
		phys_avail[j] = regions[i].mr_start;
		phys_avail[j + 1] = regions[i].mr_start + regions[i].mr_size;
		phys_avail_count++;
		physsz += regions[i].mr_size;
	}

	/* Check for overlap with the kernel and exception vectors */
	rm_pavail = 0;
	for (j = 0; j < 2*phys_avail_count; j+=2) {
		if (phys_avail[j] < EXC_LAST)
			phys_avail[j] += EXC_LAST;

		if (phys_avail[j] >= kpstart &&
		    phys_avail[j+1] <= kpend) {
			phys_avail[j] = phys_avail[j+1] = ~0;
			rm_pavail++;
			continue;
		}

		if (kpstart >= phys_avail[j] &&
		    kpstart < phys_avail[j+1]) {
			if (kpend < phys_avail[j+1]) {
				phys_avail[2*phys_avail_count] =
				    (kpend & ~PAGE_MASK) + PAGE_SIZE;
				phys_avail[2*phys_avail_count + 1] =
				    phys_avail[j+1];
				phys_avail_count++;
			}

			phys_avail[j+1] = kpstart & ~PAGE_MASK;
		}

		if (kpend >= phys_avail[j] &&
		    kpend < phys_avail[j+1]) {
			if (kpstart > phys_avail[j]) {
				phys_avail[2*phys_avail_count] = phys_avail[j];
				phys_avail[2*phys_avail_count + 1] =
				    kpstart & ~PAGE_MASK;
				phys_avail_count++;
			}

			phys_avail[j] = (kpend & ~PAGE_MASK) +
			    PAGE_SIZE;
		}
	}

	/* Remove physical available regions marked for removal (~0) */
	if (rm_pavail) {
		qsort(phys_avail, 2*phys_avail_count, sizeof(phys_avail[0]),
			pa_cmp);
		phys_avail_count -= rm_pavail;
		for (i = 2*phys_avail_count;
		     i < 2*(phys_avail_count + rm_pavail); i+=2)
			phys_avail[i] = phys_avail[i+1] = 0;
	}
	physmem = btoc(physsz);	

	bzero(kernel_pmap, sizeof(struct pmap));
	l1phys = moea64_bootstrap_alloc(RADIX_PGD_SIZE, RADIX_PGD_SIZE);
	kernel_pmap->pm_pml1 = (pml1_entry_t *)PHYS_TO_DMAP(l1phys);
	memset(kernel_pmap->pm_pml1, 0, RADIX_PGD_SIZE);
	printf("kernel_pmap pml1 %p\n", kernel_pmap->pm_pml1);
	/* XXX only supports 512G */
#if 0
	l2phys = moea64_bootstrap_alloc(PAGE_SIZE, PAGE_SIZE);
	l2virt = (uint64_t *)PHYS_TO_DMAP(l2phys);
	l2phys |= 
	kernel_pmap->pm_pml1[0] = 
#endif
}

static void
mmu_parttab_init(void)
{
	vm_paddr_t parttab_phys;
	uint64_t ptcr;

	parttab_phys = moea64_bootstrap_alloc(PARTTAB_SIZE, PARTTAB_SIZE);
	isa3_parttab = (struct pate *)PHYS_TO_DMAP(parttab_phys);

	memset(isa3_parttab, 0, PARTTAB_SIZE);
	printf("%s parttab: %p\n", __func__, isa3_parttab);
	ptcr = parttab_phys | (PARTTAB_SIZE_SHIFT-1);
	printf("setting ptcr %lx\n", ptcr);
	mtspr(SPR_PTCR, ptcr);
	printf("set ptcr\n");
	powernv_set_nmmu_ptcr(ptcr);
	printf("set nested mmu ptcr\n");
}

static void
mmu_parttab_update(uint64_t lpid, uint64_t pagetab, uint64_t proctab)
{
	uint64_t prev;
	
	printf("%s isa3_parttab %p lpid %lx pagetab %lx proctab %lx\n", __func__, isa3_parttab,
		   lpid, pagetab, proctab);
	prev = be64toh(isa3_parttab[lpid].pagetab);
	printf("%s prev = %lx\n", __func__, prev); 
	isa3_parttab[lpid].pagetab = htobe64(pagetab);
	isa3_parttab[lpid].proctab = htobe64(proctab);

	if (prev & PARTTAB_HR) {
		printf("clear old -- tlbie5\n");
		__asm __volatile(PPC_TLBIE_5(%0,%1,2,0,1) : :
			     "r" (TLBIEL_INVAL_SET_LPID), "r" (lpid));
		__asm __volatile(PPC_TLBIE_5(%0,%1,2,1,1) : :
			     "r" (TLBIEL_INVAL_SET_LPID), "r" (lpid));
	} else {
		printf("%s new value tlbie5\n", __func__);
		DELAY(10000);
		__asm __volatile(PPC_TLBIE_5(%0,%1,2,0,0) : :
			     "r" (TLBIEL_INVAL_SET_LPID), "r" (lpid));
		printf("%s tlbie5 complete\n", __func__);
		DELAY(10000);
	}
	__asm __volatile("eieio; tlbsync; ptesync" : : : "memory");
	printf("%s done\n", __func__);
}

static void
mmu_radix_parttab_init(void)
{
	uint64_t pagetab;

	mmu_parttab_init();
	printf("%s construct pagetab\n", __func__);
	pagetab = RTS_SIZE | DMAP_TO_PHYS((vm_offset_t)kernel_pmap->pm_pml1) | \
		         RADIX_PGD_INDEX_SHIFT | PARTTAB_HR;
	printf("%s install pagetab %lx\n", __func__, pagetab);
	mmu_parttab_update(0, pagetab, 0);
	printf("parttab inited\n");
}

static void
mmu_radix_proctab_register(vm_paddr_t proctabpa, uint64_t table_size)
{
	uint64_t pagetab, proctab;

	pagetab = be64toh(isa3_parttab[0].pagetab);
	proctab = proctabpa | table_size | PARTTAB_GR;
	mmu_parttab_update(0, pagetab, proctab);
}

#ifdef notyet
static int pseries_lpar_register_process_table(unsigned long base,
			unsigned long page_size, unsigned long table_size)
{
	long rc;
	unsigned long flags = 0;

	if (table_size)
		flags |= PROC_TABLE_NEW;
	if (radix_enabled())
		flags |= PROC_TABLE_RADIX | PROC_TABLE_GTSE;
	else
		flags |= PROC_TABLE_HPT_SLB;
	for (;;) {
		rc = plpar_hcall_norets(H_REGISTER_PROC_TBL, flags, base,
					page_size, table_size);
		if (!H_IS_LONG_BUSY(rc))
			break;
		mdelay(get_longbusy_msecs(rc));
	}
	if (rc != H_SUCCESS)
		panic("Failed to register process table (rc=%ld)\n", rc);
	return rc;
}
#endif

static void
mmu_radix_proctab_init(void)
{
	uint64_t parttab_size;
	vm_paddr_t proctabpa;

	/* XXX assume we're running non-virtualized and
	 * we don't support BHYVE
	 */
	if (isa3_pid_bits == 0)
		isa3_pid_bits = 20;
	isa3_base_pid = 1;


	parttab_size = 1UL << PARTTAB_SIZE_SHIFT;
	proctabpa = moea64_bootstrap_alloc(parttab_size, parttab_size);
	isa3_proctab = (void*)PHYS_TO_DMAP(proctabpa);
	isa3_proctab->proctab0 = htobe64(RTS_SIZE | DMAP_TO_PHYS((vm_offset_t)kernel_pmap->pm_pml1) | \
									 RADIX_PGD_INDEX_SHIFT);

	mmu_radix_proctab_register(proctabpa, PROCTAB_SIZE_SHIFT - 12);

	__asm __volatile("ptesync" : : : "memory");
	__asm __volatile(PPC_TLBIE_5(%0,%1,2,1,1) : :
		     "r" (TLBIEL_INVAL_SET_LPID), "r" (0));
	__asm __volatile("eieio; tlbsync; ptesync" : : : "memory");
	printf("process table %p and kernel radix PDE: %p\n",
		   isa3_proctab, kernel_pmap->pm_pml1);
	kernel_pmap->pm_pid = isa3_base_pid;
	isa3_base_pid++;
}

VISIBILITY void
METHOD(advise) pmap_t pmap, vm_offset_t start, vm_offset_t end, int advice)
{
	UNIMPLEMENTED();
}

/*
 * Routines used in machine-dependent code
 */
VISIBILITY void
METHOD(bootstrap) vm_offset_t start, vm_offset_t end)
{
	uint64_t lpcr;

	printf("%s\n", __func__);
	mmu_radix_early_bootstrap(start, end);
	printf("early bootstrap complete\n");

	if (powernv_enabled) {
		lpcr = mfspr(SPR_LPCR);
		mtspr(SPR_LPCR, lpcr | LPCR_UPRT | LPCR_HR);
		mmu_radix_parttab_init();
		mmu_radix_init_amor();
		printf("powernv init complete\n");
	} else {
		/* XXX assume we're virtualized - QEMU doesn't support radix on powernv */
		/* radix_init_pseries() */
	}
	mmu_radix_init_iamr();
	mmu_radix_proctab_init();

	mmu_radix_pid_set(kernel_pmap);
	/* XXX assume CPU_FTR_HVMODE */
	mmu_radix_tlbiel_flush(TLB_INVAL_SCOPE_GLOBAL);
}

VISIBILITY void
METHOD(clear_modify) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
}

VISIBILITY void
METHOD(copy) pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
    vm_size_t len, vm_offset_t src_addr)
{

	CTR6(KTR_PMAP, "%s(%p, %p, %#x, %#x, %#x)", __func__, dst_pmap,
	    src_pmap, dst_addr, len, src_addr);
}

VISIBILITY void
METHOD(copy_page) vm_page_t src, vm_page_t dst)
{

	CTR3(KTR_PMAP, "%s(%p, %p)", __func__, src, dst);
}

VISIBILITY void
METHOD(copy_pages) vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{

	CTR6(KTR_PMAP, "%s(%p, %#x, %p, %#x, %#x)", __func__, ma,
	    a_offset, mb, b_offset, xfersize);
}

VISIBILITY int
METHOD(enter) pmap_t pmap, vm_offset_t va, vm_page_t p, vm_prot_t prot,
    u_int flags, int8_t psind)
{

	CTR6(KTR_PMAP, "pmap_enter(%p, %#x, %p, %#x, %x, %d)", pmap, va,
	    p, prot, flags, psind);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY void
METHOD(enter_object) pmap_t pmap, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{

	CTR6(KTR_PMAP, "%s(%p, %#x, %#x, %p, %#x)", __func__, pmap, start,
	    end, m_start, prot);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(enter_quick) pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{

	CTR5(KTR_PMAP, "%s(%p, %#x, %p, %#x)", __func__, pmap, va, m, prot);

}

VISIBILITY vm_paddr_t
METHOD(extract) pmap_t pmap, vm_offset_t va)
{

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, pmap, va);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY vm_page_t
METHOD(extract_and_hold) pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{

	CTR4(KTR_PMAP, "%s(%p, %#x, %#x)", __func__, pmap, va, prot);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY void
METHOD(growkernel) vm_offset_t va)
{

	CTR2(KTR_PMAP, "%s(%#x)", __func__, va);
	UNIMPLEMENTED();
}

VISIBILITY void
METHODVOID(init)
{

	CTR1(KTR_PMAP, "%s()", __func__);
	UNIMPLEMENTED();
}

VISIBILITY boolean_t
METHOD(is_modified) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();
	return false;
}

VISIBILITY boolean_t
METHOD(is_prefaultable) pmap_t pmap, vm_offset_t va)
{

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, pmap, va);
	UNIMPLEMENTED();
	return false;
}

VISIBILITY boolean_t
METHOD(is_referenced) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY boolean_t
METHOD(ts_referenced) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY vm_offset_t
METHOD(map) vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{

	CTR5(KTR_PMAP, "%s(%p, %#x, %#x, %#x)", __func__, virt, start, end,
	    prot);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY void
METHOD(object_init_pt) pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{

	CTR6(KTR_PMAP, "%s(%p, %#x, %p, %u, %#x)", __func__, pmap, addr,
	    object, pindex, size);
	UNIMPLEMENTED();
}

VISIBILITY boolean_t
METHOD(page_exists_quick) pmap_t pmap, vm_page_t m)
{

	CTR3(KTR_PMAP, "%s(%p, %p)", __func__, pmap, m);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY void
METHOD(page_init) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();	
}

VISIBILITY int
METHOD(page_wired_mappings) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	return (0);
}

#ifdef MMU_DIRECT
int
pmap_pinit(pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	UNIMPLEMENTED();
	return (1);
}
#else
static void
mmu_radix_pinit(mmu_t mmu, pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	UNIMPLEMENTED();
}
#endif

VISIBILITY void
METHOD(pinit0) pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(protect) pmap_t pmap, vm_offset_t start, vm_offset_t end, vm_prot_t prot)
{

	CTR5(KTR_PMAP, "%s(%p, %#x, %#x, %#x)", __func__, pmap, start, end,
	    prot);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(qenter) vm_offset_t start, vm_page_t *m, int count)
{

	CTR4(KTR_PMAP, "%s(%#x, %p, %d)", __func__, start, m, count);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(qremove) vm_offset_t start, int count)
{

	CTR3(KTR_PMAP, "%s(%#x, %d)", __func__, start, count);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(release) pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(remove) pmap_t pmap, vm_offset_t start, vm_offset_t end)
{

	CTR4(KTR_PMAP, "%s(%p, %#x, %#x)", __func__, pmap, start, end);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(remove_all) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(remove_pages) pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(remove_write) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(unwire) pmap_t pmap, vm_offset_t start, vm_offset_t end)
{

	CTR4(KTR_PMAP, "%s(%p, %#x, %#x)", __func__, pmap, start, end);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(zero_page) vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(zero_page_area) vm_page_t m, int off, int size)
{

	CTR4(KTR_PMAP, "%s(%p, %d, %d)", __func__, m, off, size);
	UNIMPLEMENTED();
}

VISIBILITY int
METHOD(mincore) pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, pmap, addr);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY void
METHOD(activate) struct thread *td)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, td);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(deactivate) struct thread *td)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, td);
	UNIMPLEMENTED();
}

/*
 *	Increase the starting virtual address of the given mapping if a
 *	different alignment might result in more superpage mappings.
 */
VISIBILITY void
METHOD(align_superpage) vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{

	CTR5(KTR_PMAP, "%s(%p, %#x, %p, %#x)", __func__, object, offset, addr,
	    size);
	UNIMPLEMENTED();
}

VISIBILITY void *
METHOD(mapdev) vm_paddr_t pa, vm_size_t size)
{

	CTR3(KTR_PMAP, "%s(%#x, %#x)", __func__, pa, size);
	UNIMPLEMENTED();
	return (NULL);
}

VISIBILITY void *
METHOD(mapdev_attr) vm_paddr_t pa, vm_size_t size, vm_memattr_t attr)
{

	CTR4(KTR_PMAP, "%s(%#x, %#x, %#x)", __func__, pa, size, attr);
	UNIMPLEMENTED();
	return (NULL);
}

VISIBILITY void
METHOD(page_set_memattr) vm_page_t m, vm_memattr_t ma)
{

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, m, ma);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(unmapdev) vm_offset_t va, vm_size_t size)
{

	CTR3(KTR_PMAP, "%s(%#x, %#x)", __func__, va, size);
	UNIMPLEMENTED();
}

VISIBILITY vm_paddr_t
METHOD(kextract) vm_offset_t va)
{

	CTR2(KTR_PMAP, "%s(%#x)", __func__, va);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY void
METHOD(kenter) vm_offset_t va, vm_paddr_t pa)
{

	CTR3(KTR_PMAP, "%s(%#x, %#x)", __func__, va, pa);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(kenter_attr) vm_offset_t va, vm_paddr_t pa, vm_memattr_t ma)
{

	CTR4(KTR_PMAP, "%s(%#x, %#x, %#x)", __func__, va, pa, ma);
	UNIMPLEMENTED();
}

VISIBILITY void
METHOD(kremove) vm_offset_t va)
{

	CTR2(KTR_PMAP, "%s(%#x)", __func__, va);
	UNIMPLEMENTED();
}

VISIBILITY int
METHOD(map_user_ptr) pmap_t pm, volatile const void *uaddr, void **kaddr,
    size_t ulen, size_t *klen)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, uaddr);
	UNIMPLEMENTED();
}

VISIBILITY int
METHOD(decode_kernel_ptr) vm_offset_t addr, int *is_user, vm_offset_t *decoded)
{

	CTR2(KTR_PMAP, "%s(%#jx)", __func__, (uintmax_t)addr);
	UNIMPLEMENTED();
}

VISIBILITY boolean_t
METHOD(dev_direct_mapped) vm_paddr_t pa, vm_size_t size)
{

	CTR3(KTR_PMAP, "%s(%#x, %#x)", __func__, pa, size);
	UNIMPLEMENTED();
	return (false);
}

VISIBILITY void
METHOD(sync_icache) pmap_t pm, vm_offset_t va, vm_size_t sz)
{
 
	CTR4(KTR_PMAP, "%s(%p, %#x, %#x)", __func__, pm, va, sz);
	UNIMPLEMENTED();
}

#ifdef MMU_DIRECT
VISIBILITY void
dumpsys_map_chunk(vm_paddr_t pa, size_t sz, void **va)
{

	CTR4(KTR_PMAP, "%s(%#jx, %#zx, %p)", __func__, (uintmax_t)pa, sz, va);
	UNIMPLEMENTED();
}

void
dumpsys_unmap_chunk(vm_paddr_t pa, size_t sz, void *va)
{

	CTR4(KTR_PMAP, "%s(%#jx, %#zx, %p)", __func__, (uintmax_t)pa, sz, va);
	UNIMPLEMENTED();
}

void
dumpsys_pa_init(void)
{

	CTR1(KTR_PMAP, "%s()", __func__);
	UNIMPLEMENTED();
}
#else
static void
mmu_radix_scan_init(mmu_t mmup)
{

	CTR1(KTR_PMAP, "%s()", __func__);
	UNIMPLEMENTED();
}

static void
mmu_radix_dumpsys_map(mmu_t mmu, vm_paddr_t pa, size_t sz,
	void **va)
{
	CTR4(KTR_PMAP, "%s(%#jx, %#zx, %p)", __func__, (uintmax_t)pa, sz, va);
	UNIMPLEMENTED();	
}

#endif

VISIBILITY vm_offset_t
METHOD(quick_enter_page) vm_page_t m)
{
	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	UNIMPLEMENTED();
	return (0);
}

VISIBILITY void
METHOD(quick_remove_page) vm_offset_t addr)
{

	CTR2(KTR_PMAP, "%s(%#x)", __func__, addr);
	UNIMPLEMENTED();
}

VISIBILITY int
METHOD(change_attr) vm_offset_t addr, vm_size_t size, vm_memattr_t mode)
{
	CTR4(KTR_PMAP, "%s(%#x, %#zx, %d)", __func__, addr, size, mode);
	UNIMPLEMENTED();
	return (0);
}

#ifdef MMU_DIRECT
boolean_t
pmap_is_valid_memattr(pmap_t pmap __unused, vm_memattr_t mode)
{

	switch (mode) {
	case VM_MEMATTR_DEFAULT:
	case VM_MEMATTR_UNCACHEABLE:
	case VM_MEMATTR_CACHEABLE:
	case VM_MEMATTR_WRITE_COMBINING:
	case VM_MEMATTR_WRITE_BACK:
	case VM_MEMATTR_WRITE_THROUGH:
	case VM_MEMATTR_PREFETCHABLE:
		return (TRUE);
	default:
		return (FALSE);
	}
}
#endif
