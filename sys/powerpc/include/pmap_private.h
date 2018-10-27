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

#ifndef _MACHINE_PMAP_PRIVATE_H_
#define _MACHINE_PMAP_PRIVATE_H_

#define PG_W	RPTE_WIRED
#define PG_V	RPTE_VALID
#define PG_MANAGED	RPTE_MANAGED
#define PG_PROMOTED	RPTE_PROMOTED
#define PG_M	RPTE_C
#define PG_A	RPTE_R
#define PG_X	RPTE_EAA_X
#define PG_RW	RPTE_EAA_W
#define PG_PTE_CACHE RPTE_ATTR_MASK

#define RPTE_SHIFT 9
#define NLS_MASK ((1UL<<5)-1)
#define RPTE_ENTRIES (1UL<<RPTE_SHIFT)
#define RPTE_MASK (RPTE_ENTRIES-1)

#define NLB_SHIFT 0
#define NLB_MASK (((1UL<<52)-1) << 8)

extern int nkpt;
extern caddr_t crashdumpmap;

#define RIC_FLUSH_TLB 0
#define RIC_FLUSH_PWC 1
#define RIC_FLUSH_ALL 2

#define POWER9_TLB_SETS_RADIX	128	/* # sets in POWER9 TLB Radix mode */

#define PPC_INST_TLBIE			0x7c000264
#define PPC_INST_TLBIEL			0x7c000224
#define PPC_INST_SLBIA			0x7c0003e4

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

static __inline void
ttusync(void)
{
	__asm __volatile("eieio; tlbsync; ptesync" ::: "memory");
}

#define TLBIEL_INVAL_SEL_MASK	0xc00	/* invalidation selector */
#define  TLBIEL_INVAL_PAGE	0x000	/* invalidate a single page */
#define  TLBIEL_INVAL_SET_PID	0x400	/* invalidate a set for the current PID */
#define  TLBIEL_INVAL_SET_LPID	0x800	/* invalidate a set for current LPID */
#define  TLBIEL_INVAL_SET	0xc00	/* invalidate a set for all LPIDs */

#define TLBIE_ACTUAL_PAGE_MASK		0xe0
#define  TLBIE_ACTUAL_PAGE_4K		0x00
#define  TLBIE_ACTUAL_PAGE_64K		0xa0
#define  TLBIE_ACTUAL_PAGE_2M		0x20
#define  TLBIE_ACTUAL_PAGE_1G		0x40

#define TLBIE_PRS_PARTITION_SCOPE	0x0
#define TLBIE_PRS_PROCESS_SCOPE	0x1

#define TLBIE_RIC_INVALIDATE_TLB	0x0		/* Invalidate just TLB */
#define TLBIE_RIC_INVALIDATE_PWC	0x1		/* Invalidate just PWC */
#define TLBIE_RIC_INVALIDATE_ALL	0x2		/* Invalidate TLB, PWC,
											 * cached {proc, part}tab entries
											 */
#define TLBIE_RIC_INVALIDATE_SEQ	0x3		/* HPT - only:
											 * Invalidate a range of translations
											 */

static __inline void
radix_tlbie(uint8_t ric, uint8_t prs, uint16_t is, uint32_t pid, uint32_t lpid,
			vm_offset_t va, uint16_t ap)
{
	uint64_t rb, rs;

	MPASS((va & PAGE_MASK) == 0);

	rs = ((uint64_t)pid << 32) | lpid;
	rb = va | is | ap;
	__asm __volatile(PPC_TLBIE_5(%0, %1, %2, %3, 1) : :
		"r" (rb), "r" (rs), "i" (ric), "i" (prs));
}

static __inline void
radix_tlbie_invlpg_user_4k(uint32_t pid, vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_PAGE, pid, 0, va, TLBIE_ACTUAL_PAGE_4K);
}

static __inline void
radix_tlbie_invlpg_user_2m(uint32_t pid, vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_PAGE, pid, 0, va, TLBIE_ACTUAL_PAGE_2M);
}

static __inline void
radix_tlbie_invlpwc_user(uint32_t pid)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_PWC, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_SET_PID, pid, 0, 0, 0);
}

static __inline void
radix_tlbie_flush_user(uint32_t pid)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_ALL, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_SET_PID, pid, 0, 0, 0);
}

static __inline void
radix_tlbie_invlpg_kernel_4k(vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
				TLBIEL_INVAL_PAGE, 0, 0, va, TLBIE_ACTUAL_PAGE_4K);
}

static __inline void
radix_tlbie_invlpg_kernel_2m(vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
				TLBIEL_INVAL_PAGE, 0, 0, va, TLBIE_ACTUAL_PAGE_2M);
}

static __inline void
radix_tlbie_invlpg_kernel_1g(vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
				TLBIEL_INVAL_PAGE, 0, 0, va, TLBIE_ACTUAL_PAGE_1G);
}

static __inline void
radix_tlbie_invlpwc_kernel(void)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_PWC, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_SET_LPID, 0, 0, 0, 0);
}

static __inline void
radix_tlbie_flush_kernel(void)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_ALL, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_SET_LPID, 0, 0, 0, 0);
}

static __inline vm_pindex_t
pmap_l3e_pindex(vm_offset_t va)
{
	return ((va & PG_FRAME) >> L3_PAGE_SIZE_SHIFT);
}

static __inline vm_pindex_t
pmap_pml3e_index(vm_offset_t va)
{

	return ((va >> L3_PAGE_SIZE_SHIFT) & RPTE_MASK);
}

static __inline vm_pindex_t
pmap_pml2e_index(vm_offset_t va)
{
	return ((va >> L2_PAGE_SIZE_SHIFT) & RPTE_MASK);
}

static __inline vm_pindex_t
pmap_pml1e_index(vm_offset_t va)
{
	return ((va & PG_FRAME) >> L1_PAGE_SIZE_SHIFT);
}

/* Return various clipped indexes for a given VA */
static __inline vm_pindex_t
pmap_pte_index(vm_offset_t va)
{

	return ((va >> PAGE_SHIFT) & RPTE_MASK);
}

/* Return a pointer to the PT slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_l3e_to_pte(pt_entry_t *l3e, vm_offset_t va)
{
	pt_entry_t *pte;
	vm_paddr_t ptepa;

	ptepa = (*l3e & NLB_MASK);
	pte = (pt_entry_t *)PHYS_TO_DMAP(ptepa);
	return (&pte[pmap_pte_index(va)]);
}

/* Return a pointer to the PD slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_l2e_to_l3e(pt_entry_t *l2e, vm_offset_t va)
{
	pt_entry_t *l3e;
	vm_paddr_t l3pa;

	l3pa = (*l2e & NLB_MASK);
	l3e = (pml3_entry_t *)PHYS_TO_DMAP(l3pa);
	return (&l3e[pmap_pml3e_index(va)]);
}

/* Return a pointer to the PD slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_l1e_to_l2e(pt_entry_t *l1e, vm_offset_t va)
{
	pt_entry_t *l2e;
	vm_paddr_t l2pa;

	l2pa = (*l1e & NLB_MASK);
	
	l2e = (pml2_entry_t *)PHYS_TO_DMAP(l2pa);
	return (&l2e[pmap_pml2e_index(va)]);
}

static __inline pml1_entry_t *
pmap_pml1e(pmap_t pmap, vm_offset_t va)
{

	return (&pmap->pm_pml1[pmap_pml1e_index(va)]);
}

static pt_entry_t *
pmap_pml2e(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *l1e;

	l1e = pmap_pml1e(pmap, va);
	if (l1e == NULL || (*l1e & RPTE_VALID) == 0)
		return (NULL);
	return (pmap_l1e_to_l2e(l1e, va));
}

static __inline pt_entry_t *
pmap_pml3e(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *l2e;

	l2e = pmap_pml2e(pmap, va);
	if (l2e == NULL || (*l2e & RPTE_VALID) == 0)
		return (NULL);
	return (pmap_l2e_to_l3e(l2e, va));
}

static __inline pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *l3e;

	l3e = pmap_pml3e(pmap, va);
	if (l3e == NULL || (*l3e & RPTE_VALID) == 0)
		return (NULL);
	return (pmap_l3e_to_pte(l3e, va));
}

#endif
