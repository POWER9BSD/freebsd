#ifndef	_MACHINE_MINIDUMP_H_
#define	_MACHINE_MINIDUMP_H_ 1

#define	MINIDUMP_MAGIC		"minidump FreeBSD/powerpc64"
#define	MINIDUMP_VERSION	1

struct minidumphdr {
	char magic[24];
	uint32_t version;
	uint32_t msgbufsize;
	uint32_t bitmapsize;
	uint32_t pmapsize;
	uint64_t kernbase;
	uint64_t dmapbase;
	uint64_t dmapend;
};

#endif /* _MACHINE_MINIDUMP_H_ */
