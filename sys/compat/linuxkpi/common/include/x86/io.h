#ifndef __LKPI_X86_IO_H_
#define __LKPI_X86_IO_H_

/* XXX On Linux ioread and iowrite handle both MMIO and port IO. */
static inline void
_outb(u_char data, u_int port)
{
	__asm __volatile("outb %0, %w1" : : "a" (data), "Nd" (port));
}

#define readb_relaxed(a) __raw_readb(a)
#define readw_relaxed(a) __raw_readw(a)
#define readl_relaxed(a) __raw_readl(a)
#define readq_relaxed(a) __raw_readq(a)

#define writeb_relaxed(v, a) __raw_writeb(v, a)
#define writew_relaxed(v, a) __raw_writew(v, a)
#define writel_relaxed(v, a) __raw_writel(v, a)
#define writeq_relaxed(v, a) __raw_writeq(v, a)
#endif
