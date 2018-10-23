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
	
	
#include "assym.inc"
#include "opt_sched.h"

#include <sys/syscall.h>
#include <sys/errno.h>
	
#include <machine/trap.h>
#include <machine/spr.h>
#include <machine/param.h>
#include <machine/asm.h>

#ifdef _CALL_ELF
.abiversion _CALL_ELF
#endif

#define ENTRY_RADIX(x) ENTRY(x ## _radix)
	
.macro PROLOGUE
	mflr	%r0 
	std	%r0, 16(%r1)
.endm
	
.macro EPILOGUE
	ld	%r0, 16(%r1)
	mtlr	%r0
	blr
	nop
.endm

.macro VALIDATE_ADDR_COPY raddr
	srdi  %r0, \raddr, 52
	cmpwi %r0, 1
	bge-	copy_fault
	nop
.endm

.macro VALIDATE_ADDR_FUSU raddr
	srdi  %r0, \raddr, 52
	cmpwi %r0, 1
	bge-	fusufault
	nop
.endm
	
#define PCPU(reg) mfsprg  reg, 0

.macro SET_COPYFAULT raddr rpcb
	VALIDATE_ADDR_COPY \raddr
	PCPU(%r9)
	li %r0, 1
	ld  \rpcb, PC_CURPCB(%r9)
	std %r0, PCB_ONFAULT(\rpcb)
.endm

.macro SET_FUSUFAULT raddr rpcb
	VALIDATE_ADDR_FUSU \raddr
	PCPU(%r9)
	li  %r0, 2
	ld  \rpcb, PC_CURPCB(%r9)
	std %r0, PCB_ONFAULT(\rpcb)
.endm

.macro CLEAR_FAULT rpcb clobber
	li  %r0, 0
	std %r0, PCB_ONFAULT(\rpcb)
.if  	\clobber == 1
	li  %r3, 0
.endif
.endm

/*

 *  bcopy(src, dst, len)
 *        %r6  %r4  %r5
 * 
 *  %r3 is preserved for return in some variants
 *  %r7 is the pcb pointer
 * 
 *  %r0 and %r8-%r10 are volatile
 *
 * 	
 */

#define	rs	%r6
#define	rd	%r4
#define	rl	%r5

#define	t1	%r7
#define	t2	%r8
#define	t3	%r9
#define	t4	%r10
#define	t5	%r0
#define	t6	%r11
#define	t7	%r12
#define	t8	%r13

#define Thresh	64	
.macro SAVETMPS
	std	%r11, -8(%r1)
	std	%r12, -16(%r1)
	std	%r13, -24(%r1)
	std	%r7,  -32(%r1)
.endm	
.macro LOADTMPS
	ld	%r11, -8(%r1)
	ld	%r12, -16(%r1)
	ld	%r13, -24(%r1)
	ld	%r7,  -32(%r1)
.endm	

.text
ENTRY(bcopy_generic)
	cmplwi	0, %r5, 0
	beq	.Lend
	mr	rs, %r3
	dcbtst	0, rd
	dcbt	0, rs
	SAVETMPS
	cmplwi	rl, Thresh
	sub	t1, rs, rd
	cmplw	cr1, t1, rl	// (dst-src) < len
	blt	.Lsmall
	b	.Llarge
/* memcpy */
/* ... */
.Lsmall:
	mtcrf	0x2, rl
	mtcrf	0x1, rl
//	blt	cr1, .Lsmallrev
.Lsmall64:
	bf	26, 0f
	ld	t1, 0(rs)
	ld	t2, 8(rs)
	ld	t3, 16(rs)
	ld	t4, 24(rs)
	addi	rs, rs, 32
	std	t1, 0(rd)
	std	t2, 8(rd)
	std	t3, 16(rd)
	std	t4, 24(rd)
	addi	rd, rd, 32
0:
	bf	27, 1f
	ld	t1, 0(rs)
	ld	t2, 8(rs)
	addi	rs, rs, 16
	std	t1, 0(rd)
	std	t2, 8(rd)
	addi	rd, rd, 16
1:
	bf	28, 2f
	ld	t1, 0(rs)
	addi	rs, rs, 8
	std	t1, 0(rd)
	addi	rd, rd, 8
2:	
	bf	29, 3f
	lwz	t1, 0(rs)
	addi	rs, rs, 4
	stw	t1, 0(rd)
	addi	rd, rd, 4
3:
	bf	30, 4f
	lhz	t1, 0(rs)
	addi	rs, rs, 2
	sth	t1, 0(rd)
	addi	rd, rd, 2
4:
	bf	31, .Lout
	lbz	t1, 0(rs)
	addi	rs, rs, 1
	stb	t1, 0(rd)
	addi	rd, rd, 1
	b	.Lout

	.align 4
.Llarge:
	neg	t3, rd
	andi.	t6, t3, 0x7
//	blt	cr1, .Llargerev	
	mtctr	t6
	sub	rl, rl, t6
	beq+	.Llargealigned
1:
	lbz	t1, 0(rs)
	addi	rs, rs, 1
	stb	t1, 0(rd)
	addi	rd, rd, 1
	bdnz	1b

.Llargealigned:
	srwi.	t2, rl, 6 	// length >> 6 => 64B iterations
	mtcrf	0x2, rl
	mtcrf	0x1, rl
	beq	.Lsmall64
	mtctr	t2
	b	1f

	.align 5
1:
	ld	t1, 0(rs)
	ld	t2, 8(rs)
	ld	t3, 16(rs)
	ld	t4, 24(rs)
	ld	t5, 32(rs)
	ld	t6, 40(rs)
	ld	t7, 48(rs)
	ld	t8, 56(rs)
	addi	rs, rs, 64
	std	t1, 0(rd)
	std	t2, 8(rd)
	std	t3, 16(rd)
	std	t4, 24(rd)
	std	t5, 32(rd)
	std	t6, 40(rd)
	std	t7, 48(rd)
	std	t8, 56(rd)
	addi	rd, rd, 64
	bdnz	1b

	b	.Lsmall64
.Lout:
	LOADTMPS
/* done */	
.Lend:	
	blr

ENTRY(pagezero)
	li 	%r0, PAGE_SIZE/512
	mtctr	%r0
	li	%r4, 128
	li	%r5, 256
	li	%r6, 384
0:
	dcbz	0, %r3
	dcbz	%r4, %r3
	dcbz	%r5, %r3
	dcbz	%r6, %r3
	addi	%r3, %r3, 512
	bdnz+	0b
	blr
/*
 * copyout(from_kernel, to_user, len)
 *         %r3,        %r4,    %r5
 */
ENTRY_RADIX(copyout)
	PROLOGUE
	SET_COPYFAULT raddr=%r4 rpcb=%r7
	bl bcopy_generic 
	nop
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE

ENTRY_RADIX(copyin)
	PROLOGUE
	SET_COPYFAULT raddr=%r3 rpcb=%r7
	bl bcopy_generic
	nop
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE
/*
 * copyinstr(const void *udaddr, void *kaddr, size_t len, size_t *done)
 *			%r3          %r4         %r5        %r6 
 *
 */
	
ENTRY_RADIX(copyinstr)
	PROLOGUE
	SET_COPYFAULT raddr=%r3 rpcb=%r7
	addi	%r9, %r5, 1
	mtctr	%r9
	mr	%r8, %r3
	addi	%r8, %r8, -1
	addi	%r4, %r4, -1
	li	%r3, ENAMETOOLONG
0:
	bdz-	2f
	lbzu	%r0, 1(%r8)
	stbu	%r0, 1(%r4)

	// NULL byte reached ?
	cmpdi	%r0, 0
	beq-	1f
	b	0b
1:
	li	%r3, 0
2:
	/* skip storing length if done is NULL */
	cmpdi	%r6, 0
	beq-	3f
	mfctr	%r0
	sub	%r0, %r9, %r0
	std	%r0, 0(%r6)
3:
	CLEAR_FAULT rpcb=%r7 clobber=0
	EPILOGUE

ENTRY_RADIX(subyte)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	stb  %r4, 0(%r3)
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE

#ifndef __powerpc64__
ENTRY(suword)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	stw  %r4, 0(%r3)
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE
#endif	

ENTRY_RADIX(suword32)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	stw  %r4, 0(%r3)
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE

#ifdef __powerpc64__	
ENTRY_RADIX(suword64)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	std  %r4, 0(%r3) 
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE
ENTRY_RADIX(suword)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	std  %r4, 0(%r3) 
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE
#endif	
	
ENTRY_RADIX(fubyte)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	lbz %r3, 0(%r3)
	CLEAR_FAULT rpcb=%r7 clobber=0
	EPILOGUE

ENTRY_RADIX(fuword16)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	lhz %r3, 0(%r3)
	CLEAR_FAULT rpcb=%r7 clobber=0
	EPILOGUE

#ifndef __powerpc64__
ENTRY(fueword)	
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	lwz  %r0,, 0(%r3)
	stw  %r0,  0(%r4)
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE
#endif	
ENTRY_RADIX(fueword32)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	lwz  %r0, 0(%r3)
	stw  %r0,  0(%r4)
	CLEAR_FAULT rpcb=%r7 clobber=1
	EPILOGUE

#ifdef __powerpc64__
ENTRY_RADIX(fueword)	
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	ld  %r0, 0(%r3)
	std %r0, 0(%r4)
	CLEAR_FAULT rpcb=%r7 clobber=0
	EPILOGUE

ENTRY_RADIX(fueword64)
	PROLOGUE
	SET_FUSUFAULT raddr=%r3 rpcb=%r7
	ld  %r0, 0(%r3)
	std %r0, 0(%r4)
	CLEAR_FAULT rpcb=%r7 clobber=0
	EPILOGUE
#endif

.macro CASUEWORD32 raddr rpcb
	PROLOGUE
	SET_FUSUFAULT raddr=\raddr rpcb=\rpcb
1:
	lwarx %r0, 0, %r3
	cmplw %r4, %r0
	bne 2f
	stwcx. %r6, 0, %r3
	bne- 1b
	b 3f
2:
	stwcx. %r0, 0, %r3       	/* clear reservation (74xx) */
3:	
	CLEAR_FAULT rpcb=\rpcb clobber=0
	EPILOGUE	
.endm
	
ENTRY_RADIX(casueword32)
	CASUEWORD32 raddr=%r3 rpcb=%r7

.macro CASUEWORD64 raddr rpcb
	PROLOGUE	
	SET_FUSUFAULT raddr=\raddr rpcb=\rpcb
1:
	ldarx %r0, 0, %r3
	cmpld %r4, %r0
	bne 2f
	stdcx. %r6, 0, %r3
	bne- 1b
	b 3f
2:
	stdcx. %r0, 0, %r3       	/* clear reservation (74xx) */
3:	
	CLEAR_FAULT rpcb=\rpcb clobber=0
	EPILOGUE
.endm

#ifdef __powerpc64__
ENTRY_RADIX(casueword)
	CASUEWORD64 raddr=%r3 rpcb=%r7

ENTRY_RADIX(casueword64)
	CASUEWORD64 raddr=%r3 rpcb=%r7
#endif
	
ENTRY(fusufault)
	CLEAR_FAULT rpcb=%r7 clobber=0
	li %r3, -1
	EPILOGUE

ENTRY(copy_fault)
	CLEAR_FAULT rpcb=%r7 clobber=0
	li %r3, EFAULT
	EPILOGUE
