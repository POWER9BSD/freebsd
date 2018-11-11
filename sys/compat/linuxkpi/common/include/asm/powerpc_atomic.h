#ifndef __ASM_POWERPC_ATOMIC_H_
#define __ASM_POWERPC_ATOMIC_H_

#ifdef SMP
#define PPC_ATOMIC_ENTRY_BARRIER "\n" __XSTRING(sync) "\n"
#define PPC_ATOMIC_EXIT_BARRIER	 "\n" __XSTRING(sync) "\n"
#else
#define PPC_ATOMIC_ENTRY_BARRIER "\n"
#define PPC_ATOMIC_EXIT_BARRIER "\n"
#endif

static __always_inline uint8_t
__cmpxchg_u8(volatile uint8_t* p, uint8_t old, uint8_t new)
{
	uint8_t	prev;

	__asm __volatile (
		 PPC_ATOMIC_ENTRY_BARRIER					  
	"1: lbarx %0,0,%2       \n" // load old value \n"
		"cmpw 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"stbcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 PPC_ATOMIC_EXIT_BARRIER
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}

static __always_inline uint16_t
__cmpxchg_u16(volatile uint16_t* p, uint16_t old, uint16_t new)
{
	uint16_t	prev;

	__asm __volatile (
		 PPC_ATOMIC_ENTRY_BARRIER					  
	"1: lharx %0,0,%2       \n" // load old value\n"
		"cmpw 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"sthcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 PPC_ATOMIC_EXIT_BARRIER
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}

static __always_inline uint32_t
__cmpxchg_u32(volatile uint32_t* p, uint32_t old, uint32_t new)
{
	uint32_t	prev;

	__asm __volatile (
		 PPC_ATOMIC_ENTRY_BARRIER 
	"1: lwarx %0,0,%2       \n" // load old value\n"
		"cmpw 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"stwcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 PPC_ATOMIC_EXIT_BARRIER
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}

static __always_inline uint8_t
__cmpxchg_u8_relaxed(volatile uint8_t* p, uint8_t old, uint8_t new)
{
	uint8_t	prev;

	__asm __volatile (
	"1: lbarx %0,0,%2       \n" // load old value\n"
		"cmpw 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"stbcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}

static __always_inline uint16_t
__cmpxchg_u16_relaxed(volatile uint16_t* p, uint16_t old, uint16_t new)
{
	uint16_t	prev;

	__asm __volatile (
	"1: lharx %0,0,%2       \n" // load old value\n"
		"cmpw 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"sthcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}

static __always_inline uint32_t
__cmpxchg_u32_relaxed(volatile uint32_t* p, uint32_t old, uint32_t new)
{
	uint32_t	prev;

	__asm __volatile (
	"1: lwarx %0,0,%2       \n" // load old value\n"
		"cmpw 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"stwcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}


#ifdef __powerpc64__
static __always_inline int
__cmpxchg_u64(volatile uint64_t* p, uint64_t old, uint64_t new)
{
	uint64_t	prev;

	__asm __volatile (
		 PPC_ATOMIC_ENTRY_BARRIER					  
	"1: ldarx %0,0,%2       \n" // load old value\n"
		"cmpd 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"stdcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 PPC_ATOMIC_EXIT_BARRIER
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}

static __always_inline int
__cmpxchg_u64_relaxed(volatile uint64_t* p, uint64_t old, uint64_t new)
{
	uint64_t	prev;

	__asm __volatile (
	"1: ldarx %0,0,%2       \n" // load old value\n"
		"cmpd 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"stdcx. %4, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (old), "r" (new)
		: "cc", "memory");

	return (prev);
}

#endif

static __always_inline uint8_t
__xchg_u8_relaxed(volatile uint8_t* p, uint8_t new)
{
	uint8_t	prev;

	__asm __volatile (
	"1: lbarx %0,0,%2       \n" // load old value\n"
		"stbcx. %3, 0, %2   \n" // store new\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (new)
		: "cc");

	return (prev);
}

static __always_inline uint16_t
__xchg_u16_relaxed(volatile uint16_t* p, uint16_t new)
{
	uint16_t	prev;

	__asm __volatile (
	"1: lharx %0,0,%2       \n" // load old value\n"
		"sthcx. %3, 0, %2   \n" // store new\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (new)
		: "cc");

	return (prev);
}

static __always_inline uint32_t
__xchg_u32_relaxed(volatile uint32_t* p, uint32_t new)
{
	uint32_t	prev;

	__asm __volatile (
	"1: lwarx %0,0,%2       \n" // load old value\n"
		"stwcx. %3, 0, %2   \n" // store new\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (new)
		: "cc");

	return (prev);
}

#ifdef __powerpc64__
static __always_inline uint64_t
__xchg_u64_relaxed(volatile uint64_t* p, uint64_t new)
{
	int	prev;

	__asm __volatile (
	"1: ldarx %0,0,%2       \n" // load old value\n"
		"cmpd 0,%0,%3 	    \n" // compare\n"
		"bne- 2f		    \n" // exit if not equal\n"
		"stdcx. %3, 0, %2   \n" // attempt to store\n"
		"bne- 1b			\n" // retry if failed\n"
		 "2:" 
		: "=&r" (prev), "+m" (*p)
		: "r" (p), "r" (new)
		: "cc");

	return (prev);
}
#endif
#ifdef __powerpc64__
#define	LINUXKPI_ATOMIC_64(...) __VA_ARGS__
#else
#define	LINUXKPI_ATOMIC_64(...)
#endif

#define	cmpxchg(ptr, old, new) ({					\
	union {								\
		__typeof(*(ptr)) val;					\
		u8 u8;						\
		u16 u16;						\
		u32 u32;						\
		u64 u64;						\
	} __ret = { .val = (old) }, __new = { .val = (new) };		\
									\
									\
	switch (sizeof(__ret.val)) {					\
	case 1:								\
		__cmpxchg_u8((volatile u8*)ptr,	__ret.u8, __new.u8);	\
		break;							\
	case 2:								\
		__cmpxchg_u16((volatile u16*)ptr,	__ret.u16, __new.u16);	\
		break;							\
	case 4:								\
		__cmpxchg_u32((volatile u32*)ptr,	__ret.u32, __new.u32);	\
		break;							\
    LINUXKPI_ATOMIC_64(				\
	case 8:								\
		__cmpxchg_u64((volatile u64*)ptr,	__ret.u64, __new.u64);	\
		break;							\
		)								\
	    }								\
	__ret.val;							\
})


#define	cmpxchg_relaxed(ptr, old, new) ({					\
	union {								\
		__typeof(*(ptr)) val;					\
		u8 u8;						\
		u16 u16;						\
		u32 u32;						\
		u64 u64;						\
	} __ret = { .val = (old) }, __new = { .val = (new) };		\
									\
									\
	switch (sizeof(__ret.val)) {					\
	case 1:								\
		__cmpxchg_u8_relaxed((volatile u8*)ptr,	__ret.u8, __new.u8);	\
		break;							\
	case 2:								\
		__cmpxchg_u16_relaxed((volatile u16*)ptr,	__ret.u16, __new.u16);	\
		break;							\
	case 4:								\
		__cmpxchg_u32_relaxed((volatile u32*)ptr,	__ret.u32, __new.u32);	\
		break;							\
    LINUXKPI_ATOMIC_64(				\
	case 8:								\
		__cmpxchg_u64_relaxed((volatile u64*)ptr,	__ret.u64, __new.u64);	\
		break;							\
		)								\
	    }								\
	__ret.val;							\
})

#define	xchg_relaxed(ptr, new) ({						\
	union {								\
		__typeof(*(ptr)) val;					\
		u8 u8;						\
		u16 u16;						\
		u32 u32;						\
		u64 u64;						\
	} __ret, __new = { .val = (new) };				\
	switch (sizeof(__ret.val)) {					\
	case 1:								\
		__ret.u8 = __xchg_u8_relaxed((volatile u8 *)(ptr), __new.u8); \
		break;															\
	case 2:								\
		__ret.u16 = __xchg_u16_relaxed((volatile u16 *)(ptr), __new.u16); \
		break;							\
	case 4:								\
		__ret.u32 = __xchg_u32_relaxed((volatile u32 *)(ptr), __new.u32); \
		break;							\
	LINUXKPI_ATOMIC_64(						\
	case 8:								\
	__ret.u64 = __xchg_u64_relaxed((volatile u64 *)(ptr), __new.u64); \
		break;							\
	)								\
	}								\
	__ret.val;							\
})

#define xchg(ptr, new) ({						\
    __typeof(*(ptr)) v;					\
	__asm __volatile("sync");							\
	v = xchg_relaxed(ptr, new);					\
	__asm __volatile("sync");							\
	v;											\
  })



#define atomic_cmpxchg(v, o, n) (cmpxchg(&((v)->counter), (o), (n)))
#define atomic_cmpxchg_relaxed(v, o, n) \
	cmpxchg_relaxed(&((v)->counter), (o), (n))

#define atomic_xchg(v, new) (xchg(&((v)->counter), new))
#define atomic_xchg_relaxed(v, new) xchg_relaxed(&((v)->counter), (new))
#endif
