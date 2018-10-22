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

static __inline void
ttusync(void)
{
	__asm __volatile("eieio; tlbsync; ptesync" ::: "memory");
}

static __inline void
tlbie(vm_offset_t va) {
	__asm __volatile("tlbie %0" :: "r"(va) : "memory");
	ttusync();
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
