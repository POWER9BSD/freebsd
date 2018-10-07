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
#include <sys/kerneldump.h>
#include <sys/ktr.h>
#include <sys/lock.h>
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
#define METHOD(m) pmap_ ## m
#define METHOD(m) dumpsys_ ## m
#define VISIBILITY
#endif

#define UNIMPLEMENTED() panic("%s not implemented", __func__)


VISIBILITY void
METHOD(advise) pmap_t pmap, vm_offset_t start, vm_offset_t end, int advice)
{
	UNIMPLEMENTED();
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

/*
 * Routines used in machine-dependent code
 */
VISIBILITY void
METHOD(bootstrap) vm_offset_t start, vm_offset_t end)
{

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
