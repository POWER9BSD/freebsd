/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_IO_H_
#define	_LINUX_IO_H_

#include <machine/vm.h>
#include <sys/endian.h>
#include <sys/types.h>

#include <linux/compiler.h>
#include <linux/types.h>

#undef readb
#undef writeb
#undef readw
#undef writew
#undef readl
#undef writel
#undef readq
#undef writeq

#ifdef __powerpc__
#include <powerpc/io.h>
#elif defined(__i386__) || defined(__amd64__)
#include <x86/io.h>
#endif

#ifndef mmiowb
#define	mmiowb()	barrier()
#endif

#ifndef __raw_readb
static inline uint8_t
__raw_readb(const volatile void *addr)
{
	return (*(const volatile uint8_t *)addr);
}
#define	__raw_readb(addr)	__raw_readb(addr)
#endif

#ifndef __raw_writeb
static inline void
__raw_writeb(uint8_t v, volatile void *addr)
{
	*(volatile uint8_t *)addr = v;
}
#define	__raw_writeb(v, addr)	__raw_writeb(v, addr)
#endif

#ifndef __raw_readw
static inline uint16_t
__raw_readw(const volatile void *addr)
{
	return (*(const volatile uint16_t *)addr);
}
#define	__raw_readw(addr)	__raw_readw(addr)
#endif

#ifndef __raw_writew
static inline void
__raw_writew(uint16_t v, volatile void *addr)
{
	*(volatile uint16_t *)addr = v;
}
#define	__raw_writew(v, addr)	__raw_writew(v, addr)
#endif

#ifndef __raw_readl
static inline uint32_t
__raw_readl(const volatile void *addr)
{
	return (*(const volatile uint32_t *)addr);
}
#define	__raw_readl(addr)	__raw_readl(addr)
#endif

#ifndef __raw_writel
static inline void
__raw_writel(uint32_t v, volatile void *addr)
{
	*(volatile uint32_t *)addr = v;
}
#define	__raw_writel(v, addr)	__raw_writel(v, addr)
#endif

#ifdef __LP64__
#ifndef __raw_readq
static inline uint64_t
__raw_readq(const volatile void *addr)
{
	return (*(const volatile uint64_t *)addr);
}
#define	__raw_readq(addr)	__raw_readq(addr)
#endif

#ifndef __raw_writeq
static inline void
__raw_writeq(uint64_t v, volatile void *addr)
{
	*(volatile uint64_t *)addr = v;
}
#define	__raw_writeq(v, addr)	__raw_writeq(v, addr)
#endif
#endif /* __LP64__ */


#ifndef __io_br
#define __io_br()      __compiler_membar();
#endif

/* prevent prefetching of coherent DMA data ahead of a dma-complete */
#ifndef __io_ar
#ifdef rmb
#define __io_ar()      rmb()
#else
#define __io_ar()      __compiler_membar();
#endif
#endif

/* flush writes to coherent DMA data before possibly triggering a DMA read */
#ifndef __io_bw
#ifdef wmb
#define __io_bw()      wmb()
#else
#define __io_bw()      __compiler_membar();
#endif
#endif

/* serialize device access against a spin_unlock, usually handled there. */
#ifndef __io_aw
#define __io_aw()      __compiler_membar();
#endif

/* Access little-endian MMIO registers atomically with memory barriers. */
#ifndef readb
static inline uint8_t
readb(const volatile void *addr)
{
	uint8_t v;

	__io_br();
	v = __raw_readb(addr);
	__io_ar();
	return (v);
}
#define	readb(addr)		readb(addr)
#endif

#ifndef writeb
static inline void
writeb(uint8_t v, volatile void *addr)
{
	__io_bw();
	__raw_writeb(v, addr);
	__io_aw();
}
#define	writeb(v, addr)		writeb(v, addr)
#endif

#ifndef readw
static inline uint16_t
readw(const volatile void *addr)
{
	uint16_t v;

	__io_br();
	v = le16toh(__raw_readw(addr));
	__io_ar();
	return (v);
}
#define	readw(addr)		readw(addr)
#endif

#ifndef writew
static inline void
writew(uint16_t v, volatile void *addr)
{
	__io_bw();
	__raw_writew(htole16(v), addr);
	__io_aw();
}
#define	writew(v, addr)		writew(v, addr)
#endif

#ifndef readl
static inline uint32_t
readl(const volatile void *addr)
{
	uint32_t v;

	__io_br();
	v = le32toh(__raw_readl(addr));
	__io_ar();
	return (v);
}
#define	readl(addr)		readl(addr)
#endif

#ifndef writel
static inline void
writel(uint32_t v, volatile void *addr)
{
	__io_bw();
	__raw_writel(htole32(v), addr);
	__io_aw();
}
#define	writel(v, addr)		writel(v, addr)
#endif

#ifdef __LP64__
#ifndef readq
static inline uint64_t
readq(const volatile void *addr)
{
	uint64_t v;

	__io_br();
	v = le64toh(__raw_readq(addr));
	__io_ar();
	return (v);
}
#define	readq(addr)		readq(addr)
#endif

#ifndef writeq
static inline void
writeq(uint64_t v, volatile void *addr)
{
	__io_bw();
	__raw_writeq(htole64(v), addr);
	__io_aw();
}
#define	writeq(v, addr)		writeq(v, addr)
#endif

#endif /* __LP64__ */


#ifndef ioread8
static inline uint8_t
ioread8(const volatile void *addr)
{
	return (readb(addr));
}
#define	ioread8(addr)		ioread8(addr)
#endif

#ifndef ioread16
static inline uint16_t
ioread16(const volatile void *addr)
{
	return (readw(addr));
}
#define	ioread16(addr)		ioread16(addr)
#endif

#ifndef ioread16be
static inline uint16_t
ioread16be(const volatile void *addr)
{
	return (bswap16(readw(addr)));
}
#define	ioread16be(addr)	ioread16be(addr)
#endif

#ifndef ioread32
static inline uint32_t
ioread32(const volatile void *addr)
{
	return (readl(addr));
}
#define	ioread32(addr)		ioread32(addr)
#endif

#ifndef ioread32be
static inline uint32_t
ioread32be(const volatile void *addr)
{
	return (bswap32(readl(addr)));
}
#define	ioread32be(addr)	ioread32be(addr)
#endif

#ifndef iowrite8
static inline void
iowrite8(uint8_t v, volatile void *addr)
{
	writeb(v, addr);
}
#define	iowrite8(v, addr)	iowrite8(v, addr)
#endif

#ifndef iowrite16
static inline void
iowrite16(uint16_t v, volatile void *addr)
{
	writew(v, addr);
}
#define	iowrite16	iowrite16
#endif

#ifndef iowrite32
static inline void
iowrite32(uint32_t v, volatile void *addr)
{
	writel(v, addr);
}
#define	iowrite32(v, addr)	iowrite32(v, addr)
#endif

#ifndef iowrite32be
static inline void
iowrite32be(uint32_t v, volatile void *addr)
{
	writel(bswap32(v), addr);
}
#define	iowrite32be(v, addr)	iowrite32be(v, addr)
#endif


#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__)
void *_ioremap_attr(vm_paddr_t phys_addr, unsigned long size, int attr);
#else
#define	_ioremap_attr(...) NULL
#endif

#define	ioremap_nocache(addr, size)					\
    _ioremap_attr((addr), (size), VM_MEMATTR_UNCACHEABLE)
#define	ioremap_wc(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_WRITE_COMBINING)
#define	ioremap_wb(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_WRITE_BACK)
#define	ioremap_wt(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_WRITE_THROUGH)
#define	ioremap(addr, size)						\
    _ioremap_attr((addr), (size), VM_MEMATTR_UNCACHEABLE)
void iounmap(void *addr);

#define	memset_io(a, b, c)	memset((a), (b), (c))
#define	memcpy_fromio(a, b, c)	memcpy((a), (b), (c))
#define	memcpy_toio(a, b, c)	memcpy((a), (b), (c))

static inline void
__iowrite32_copy(void *to, void *from, size_t count)
{
	uint32_t *src;
	uint32_t *dst;
	int i;

	for (i = 0, src = from, dst = to; i < count; i++, src++, dst++)
		__raw_writel(*src, dst);
}

static inline void
__iowrite64_copy(void *to, void *from, size_t count)
{
#ifdef __LP64__
	uint64_t *src;
	uint64_t *dst;
	int i;

	for (i = 0, src = from, dst = to; i < count; i++, src++, dst++)
		__raw_writeq(*src, dst);
#else
	__iowrite32_copy(to, from, count * 2);
#endif
}

enum {
	MEMREMAP_WB = 1 << 0,
	MEMREMAP_WT = 1 << 1,
	MEMREMAP_WC = 1 << 2,
};

static inline void *
memremap(resource_size_t offset, size_t size, unsigned long flags)
{
	void *addr = NULL;

	if ((flags & MEMREMAP_WB) &&
	    (addr = ioremap_wb(offset, size)) != NULL)
		goto done;
	if ((flags & MEMREMAP_WT) &&
	    (addr = ioremap_wt(offset, size)) != NULL)
		goto done;
	if ((flags & MEMREMAP_WC) &&
	    (addr = ioremap_wc(offset, size)) != NULL)
		goto done;
done:
	return (addr);
}

static inline void
memunmap(void *addr)
{
	/* XXX May need to check if this is RAM */
	iounmap(addr);
}

#endif	/* _LINUX_IO_H_ */
