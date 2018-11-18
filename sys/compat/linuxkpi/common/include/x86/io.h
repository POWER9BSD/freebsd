#ifndef __LKPI_X86_IO_H_
#define __LKPI_X86_IO_H_

/* XXX On Linux ioread and iowrite handle both MMIO and port IO. */
static inline void
_outb(u_char data, u_int port)
{
	__asm __volatile("outb %0, %w1" : : "a" (data), "Nd" (port));
}
#endif
