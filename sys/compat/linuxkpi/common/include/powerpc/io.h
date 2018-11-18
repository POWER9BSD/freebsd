#ifndef __LKPI_POWERPC_IO_H_
#define __LKPI_POWERPC_IO_H_


#ifdef __powerpc64__
#define mmiowb()		__asm __volatile("sync" : : : "memory")
#else
#define mmiowb()
#endif

#define readb_relaxed(addr)	readb(addr)
#define readw_relaxed(addr)	readw(addr)
#define readl_relaxed(addr)	readl(addr)
#define readq_relaxed(addr)	readq(addr)
#define writeb_relaxed(v, addr)	writeb(v, addr)
#define writew_relaxed(v, addr)	writew(v, addr)
#define writel_relaxed(v, addr)	writel(v, addr)
#define writeq_relaxed(v, addr)	writeq(v, addr)

#endif
