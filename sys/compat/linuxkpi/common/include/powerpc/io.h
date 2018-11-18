#ifndef __LKPI_POWERPC_IO_H_
#define __LKPI_POWERPC_IO_H_


#ifdef __powerpc64__
#define mmiowb()		__asm __volatile("sync" : : : "memory")
#else
#define mmiowb()
#endif
#endif
