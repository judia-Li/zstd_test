typedef uint 			U32;
typedef ulong 		U64;

#define FAST_HASH_FILL_STEP 	(3)

#define prime8bytes 0xCF1BBCDCB7A56463ULL
inline U32 ZSTD_hash8(U64 u, U32 h) 
{ return (U32)(((u) * prime8bytes) >> (64-h)) ; }

#define prime5bytes 889523592379ULL
inline U32 ZSTD_hash5(U64 u, U32 h) 
{ return (U32)(((u  << (64-40)) * prime5bytes) >> (64-h)) ; }

typedef union
{
	uchar8	v;
	U64		s;
}UnionData;

kernel void  ZSTD_fillDoubleHashTable_cl(
											const global uchar* ip,
											global U32*restrict hashLargeValTable,
											global U32*restrict hashSmallValTable,
											U32 hBitsL,
											U32 hBitsS)
{
	const uint gId = get_global_id(0);
	const uint gId2 = gId * FAST_HASH_FILL_STEP;

	uchar8 data8 = vload8(0, ip + gId2);
	uchar2 data2 = vload2(0, ip + gId2 + 8);

	ushort ushort0 = upsample(data8.s1, data8.s0);
	ushort ushort1 = upsample(data8.s3, data8.s2);
	ushort ushort2 = upsample(data8.s5, data8.s4);
	ushort ushort3 = upsample(data8.s7, data8.s6);
	
	uint uint0 = upsample(ushort1, ushort0);
	uint uint1 = upsample(ushort3, ushort2);

	U64 value = upsample(uint1, uint0);

	hashSmallValTable[gId] = ZSTD_hash5(value, hBitsS);
	hashLargeValTable[gId2] = ZSTD_hash8(value, hBitsL);

	ushort0 = upsample(data8.s2, data8.s1);
	ushort1 = upsample(data8.s4, data8.s3);
	ushort2 = upsample(data8.s6, data8.s5);
	ushort3 = upsample(data2.s0, data8.s7);
	
	uint0 = upsample(ushort1, ushort0);
	uint1 = upsample(ushort3, ushort2);

	value = upsample(uint1, uint0);
	hashLargeValTable[gId2 + 1] = ZSTD_hash8(value, hBitsL);

	ushort0 = upsample(data8.s3, data8.s2);
	ushort1 = upsample(data8.s5, data8.s4);
	ushort2 = upsample(data8.s7, data8.s6);
	ushort3 = upsample(data2.s1, data2.s0);
	
	uint0 = upsample(ushort1, ushort0);
	uint1 = upsample(ushort3, ushort2);

	value = upsample(uint1, uint0);
	hashLargeValTable[gId2 + 2] = ZSTD_hash8(value, hBitsL);
}
