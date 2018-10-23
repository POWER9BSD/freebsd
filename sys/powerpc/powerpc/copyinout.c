/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD AND BSD-4-Clause
 *
 * Copyright (C) 2002 Benno Rice
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*-
 * Copyright (C) 1993 Wolfgang Solfrank.
 * Copyright (C) 1993 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/pcb.h>
#include <machine/vmparam.h>
#include <machine/ifunc.h>

#ifdef __powerpc64__
int subyte_hash(volatile void *addr, int byte);
int subyte_radix(volatile void *addr, int byte);
int copyinstr_hash(const void *udaddr, void *kaddr, size_t len, size_t *done);
int copyinstr_radix(const void *udaddr, void *kaddr, size_t len, size_t *done);
int copyout_hash(const void *kaddr, void *udaddr, size_t len);
int copyout_radix(const void *kaddr, void *udaddr, size_t len);
int copyin_hash(const void *uaddr, void *kaddr, size_t len);
int copyin_radix(const void *uaddr, void *kaddr, size_t len);
int suword32_hash(volatile void *addr, int word);
int suword32_radix(volatile void *addr, int word);
int suword_hash(volatile void *addr, long word);
int suword_radix(volatile void *addr, long word);
int suword64_hash(volatile void *addr, int64_t word);
int suword64_radix(volatile void *addr, int64_t word);
int fubyte_hash(volatile const void *addr);
int fubyte_radix(volatile const void *addr);
int fuword16_hash(volatile const void *addr);
int fuword16_radix(volatile const void *addr);
int fueword32_hash(volatile const void *addr, int32_t *val);
int fueword32_radix(volatile const void *addr, int32_t *val);
int fueword64_hash(volatile const void *addr, int64_t *val);
int fueword64_radix(volatile const void *addr, int64_t *val);
int fueword_hash(volatile const void *addr, long *val);
int fueword_radix(volatile const void *addr, long *val);
int casueword32_hash(volatile uint32_t *addr, uint32_t old, uint32_t *oldvalp,
	uint32_t new);
int casueword32_radix(volatile uint32_t *addr, uint32_t old, uint32_t *oldvalp,
	uint32_t new);
int casueword_hash(volatile u_long *addr, u_long old, u_long *oldvalp,
	u_long new);
int casueword_radix(volatile u_long *addr, u_long old, u_long *oldvalp,
	u_long new);
#define FUNCNAME(x) x ## _hash

DEFINE_IFUNC(, int, subyte, (volatile void *, int), static)
{

	return (disable_radix ?
	    subyte_hash : subyte_radix);
}
DEFINE_IFUNC(, int, copyinstr, (const void *, void *, size_t, size_t *), static)
{

	return (disable_radix ?
	    copyinstr_hash : copyinstr_radix);
}
DEFINE_IFUNC(, int, copyin, (const void *, void *, size_t), static)
{

	return (disable_radix ?
	    copyin_hash : copyin_radix);
}
DEFINE_IFUNC(, int, copyout, (const void *, void *, size_t), static)
{

	return (disable_radix ?
	    copyout_hash : copyout_radix);
}
DEFINE_IFUNC(, int, suword, (volatile void *, long), static)
{

	return (disable_radix ?
	    suword_hash : suword_radix);
}
DEFINE_IFUNC(, int, suword32, (volatile void *, int), static)
{

	return (disable_radix ?
	    suword32_hash : suword32_radix);
}
DEFINE_IFUNC(, int, suword64, (volatile void *, int64_t), static)
{

	return (disable_radix ?
			suword64_hash : suword64_radix);
}
DEFINE_IFUNC(, int, fubyte, (volatile const void *), static)
{

	return (disable_radix ?
	    fubyte_hash : fubyte_radix);
}
DEFINE_IFUNC(, int, fuword16, (volatile const void *), static)
{

	return (disable_radix ?
	    fuword16_hash : fuword16_radix);
}
DEFINE_IFUNC(, int, fueword32, (volatile const void *, int32_t *), static)
{

	return (disable_radix ?
	    fueword32_hash : fueword32_radix);
}
DEFINE_IFUNC(, int, fueword64, (volatile const void *, int64_t *), static)
{

	return (disable_radix ?
	    fueword64_hash : fueword64_radix);
}
DEFINE_IFUNC(, int, fueword, (volatile const void *, long *), static)
{

	return (disable_radix ?
	    fueword_hash : fueword_radix);
}
DEFINE_IFUNC(, int, casueword32, (volatile uint32_t *, uint32_t, uint32_t *, uint32_t), static)
{

	return (disable_radix ?
	    casueword32_hash : casueword32_radix);
}
DEFINE_IFUNC(, int, casueword, (volatile u_long *, u_long, u_long *, u_long), static)
{

	return (disable_radix ?
	    casueword_hash : casueword_radix);
}

#else
#define FUNCNAME(x) x 
#endif


int
FUNCNAME(copyout)(const void *kaddr, void *udaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	const char	*kp;
	char		*up, *p;
	size_t		l;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		if (pmap_map_user_ptr(pm, up, (void **)&p, len, &l)) {
			td->td_pcb->pcb_onfault = NULL;
			return (EFAULT);
		}

		bcopy(kp, p, l);

		up += l;
		kp += l;
		len -= l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
FUNCNAME(copyin)(const void *udaddr, void *kaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	const char	*up;
	char		*kp, *p;
	size_t		l;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		if (pmap_map_user_ptr(pm, up, (void **)&p, len, &l)) {
			td->td_pcb->pcb_onfault = NULL;
			return (EFAULT);
		}

		bcopy(p, kp, l);

		up += l;
		kp += l;
		len -= l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
FUNCNAME(copyinstr)(const void *udaddr, void *kaddr, size_t len, size_t *done)
{
	const char	*up;
	char		*kp;
	size_t		l;
	int		rv, c;

	kp = kaddr;
	up = udaddr;

	rv = ENAMETOOLONG;

	for (l = 0; len-- > 0; l++) {
		if ((c = fubyte(up++)) < 0) {
			rv = EFAULT;
			break;
		}

		if (!(*kp++ = c)) {
			l++;
			rv = 0;
			break;
		}
	}

	if (done != NULL) {
		*done = l;
	}

	return (rv);
}

int
FUNCNAME(subyte)(volatile void *addr, int byte)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	char		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*p = (char)byte;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
suword32_hash(volatile void *addr, int word)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	int		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*p = word;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}
#endif

int
FUNCNAME(suword)(volatile void *addr, long word)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	long		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*p = word;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
suword64_hash(volatile void *addr, int64_t word)
{
	return (suword_hash(addr, (long)word));
}
#else
int
suword32(volatile void *addr, int32_t word)
{
	return (suword(addr, (long)word));
}
#endif

int
FUNCNAME(fubyte)(volatile const void *addr)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	u_char		*p;
	int		val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

int
FUNCNAME(fuword16)(volatile const void *addr)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	uint16_t	*p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

int
FUNCNAME(fueword32)(volatile const void *addr, int32_t *val)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	int32_t		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
fueword64_hash(volatile const void *addr, int64_t *val)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	int64_t		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}
#endif

int
FUNCNAME(fueword)(volatile const void *addr, long *val)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	long		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
FUNCNAME(casueword32)(volatile uint32_t *addr, uint32_t old, uint32_t *oldvalp,
    uint32_t new)
{
	struct thread *td;
	pmap_t pm;
	jmp_buf		env;
	uint32_t *p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, (void *)(uintptr_t)addr, (void **)&p,
	    sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"cmplw %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stwcx. %4, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stwcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
		"3:\n\t"
		: "=&r" (val), "=m" (*p)
		: "r" (p), "r" (old), "r" (new), "m" (*p)
		: "cr0", "memory");

	td->td_pcb->pcb_onfault = NULL;

	*oldvalp = val;
	return (0);
}

#ifndef __powerpc64__
int
casueword(volatile u_long *addr, u_long old, u_long *oldvalp, u_long new)
{

	return (casueword32((volatile uint32_t *)addr, old,
	    (uint32_t *)oldvalp, new));
}
#else
int
casueword_hash(volatile u_long *addr, u_long old, u_long *oldvalp, u_long new)
{
	struct thread *td;
	pmap_t pm;
	jmp_buf		env;
	u_long *p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, (void *)(uintptr_t)addr, (void **)&p,
	    sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	__asm __volatile (
		"1:\tldarx %0, 0, %2\n\t"	/* load old value */
		"cmpld %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stdcx. %4, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stdcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
		"3:\n\t"
		: "=&r" (val), "=m" (*p)
		: "r" (p), "r" (old), "r" (new), "m" (*p)
		: "cr0", "memory");

	td->td_pcb->pcb_onfault = NULL;

	*oldvalp = val;
	return (0);
}
#endif
