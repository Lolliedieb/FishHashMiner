#define WORKSIZE 128


/****************************************************

	Kernel for the dataset generation

    This is taken from Ethminer with a modification
   of the number of parent elements of the DAG items

****************************************************/

#define FNV_PRIME 0x01000193U

static __constant uint2 const Keccak_f1600_RC[24] = {
    (uint2)(0x00000001, 0x00000000),
    (uint2)(0x00008082, 0x00000000),
    (uint2)(0x0000808a, 0x80000000),
    (uint2)(0x80008000, 0x80000000),
    (uint2)(0x0000808b, 0x00000000),
    (uint2)(0x80000001, 0x00000000),
    (uint2)(0x80008081, 0x80000000),
    (uint2)(0x00008009, 0x80000000),
    (uint2)(0x0000008a, 0x00000000),
    (uint2)(0x00000088, 0x00000000),
    (uint2)(0x80008009, 0x00000000),
    (uint2)(0x8000000a, 0x00000000),
    (uint2)(0x8000808b, 0x00000000),
    (uint2)(0x0000008b, 0x80000000),
    (uint2)(0x00008089, 0x80000000),
    (uint2)(0x00008003, 0x80000000),
    (uint2)(0x00008002, 0x80000000),
    (uint2)(0x00000080, 0x80000000),
    (uint2)(0x0000800a, 0x00000000),
    (uint2)(0x8000000a, 0x80000000),
    (uint2)(0x80008081, 0x80000000),
    (uint2)(0x00008080, 0x80000000),
    (uint2)(0x80000001, 0x00000000),
    (uint2)(0x80008008, 0x80000000),
};

#ifdef cl_amd_media_ops

#define ROTL64_1(x, y) amd_bitalign((x), (x).s10, 32 - (y))
#define ROTL64_2(x, y) amd_bitalign((x).s10, (x), 32 - (y))

#else

#define ROTL64_1(x, y) as_uint2(rotate(as_ulong(x), (ulong)(y)))
#define ROTL64_2(x, y) ROTL64_1(x, (y) + 32)

#endif


#define KECCAKF_1600_RND(a, i, outsz) do { \
    const uint2 m0 = a[0] ^ a[5] ^ a[10] ^ a[15] ^ a[20] ^ ROTL64_1(a[2] ^ a[7] ^ a[12] ^ a[17] ^ a[22], 1);\
    const uint2 m1 = a[1] ^ a[6] ^ a[11] ^ a[16] ^ a[21] ^ ROTL64_1(a[3] ^ a[8] ^ a[13] ^ a[18] ^ a[23], 1);\
    const uint2 m2 = a[2] ^ a[7] ^ a[12] ^ a[17] ^ a[22] ^ ROTL64_1(a[4] ^ a[9] ^ a[14] ^ a[19] ^ a[24], 1);\
    const uint2 m3 = a[3] ^ a[8] ^ a[13] ^ a[18] ^ a[23] ^ ROTL64_1(a[0] ^ a[5] ^ a[10] ^ a[15] ^ a[20], 1);\
    const uint2 m4 = a[4] ^ a[9] ^ a[14] ^ a[19] ^ a[24] ^ ROTL64_1(a[1] ^ a[6] ^ a[11] ^ a[16] ^ a[21], 1);\
    \
    const uint2 tmp = a[1]^m0;\
    \
    a[0] ^= m4;\
    a[5] ^= m4; \
    a[10] ^= m4; \
    a[15] ^= m4; \
    a[20] ^= m4; \
    \
    a[6] ^= m0; \
    a[11] ^= m0; \
    a[16] ^= m0; \
    a[21] ^= m0; \
    \
    a[2] ^= m1; \
    a[7] ^= m1; \
    a[12] ^= m1; \
    a[17] ^= m1; \
    a[22] ^= m1; \
    \
    a[3] ^= m2; \
    a[8] ^= m2; \
    a[13] ^= m2; \
    a[18] ^= m2; \
    a[23] ^= m2; \
    \
    a[4] ^= m3; \
    a[9] ^= m3; \
    a[14] ^= m3; \
    a[19] ^= m3; \
    a[24] ^= m3; \
    \
    a[1] = ROTL64_2(a[6], 12);\
    a[6] = ROTL64_1(a[9], 20);\
    a[9] = ROTL64_2(a[22], 29);\
    a[22] = ROTL64_2(a[14], 7);\
    a[14] = ROTL64_1(a[20], 18);\
    a[20] = ROTL64_2(a[2], 30);\
    a[2] = ROTL64_2(a[12], 11);\
    a[12] = ROTL64_1(a[13], 25);\
    a[13] = ROTL64_1(a[19],  8);\
    a[19] = ROTL64_2(a[23], 24);\
    a[23] = ROTL64_2(a[15], 9);\
    a[15] = ROTL64_1(a[4], 27);\
    a[4] = ROTL64_1(a[24], 14);\
    a[24] = ROTL64_1(a[21],  2);\
    a[21] = ROTL64_2(a[8], 23);\
    a[8] = ROTL64_2(a[16], 13);\
    a[16] = ROTL64_2(a[5], 4);\
    a[5] = ROTL64_1(a[3], 28);\
    a[3] = ROTL64_1(a[18], 21);\
    a[18] = ROTL64_1(a[17], 15);\
    a[17] = ROTL64_1(a[11], 10);\
    a[11] = ROTL64_1(a[7],  6);\
    a[7] = ROTL64_1(a[10],  3);\
    a[10] = ROTL64_1(tmp,  1);\
    \
    uint2 m5 = a[0]; uint2 m6 = a[1]; a[0] = bitselect(a[0]^a[2],a[0],a[1]); \
    a[0] ^= as_uint2(Keccak_f1600_RC[i]); \
    if (outsz > 1) { \
        a[1] = bitselect(a[1]^a[3],a[1],a[2]); a[2] = bitselect(a[2]^a[4],a[2],a[3]); a[3] = bitselect(a[3]^m5,a[3],a[4]); a[4] = bitselect(a[4]^m6,a[4],m5);\
        if (outsz > 4) { \
            m5 = a[5]; m6 = a[6]; a[5] = bitselect(a[5]^a[7],a[5],a[6]); a[6] = bitselect(a[6]^a[8],a[6],a[7]); a[7] = bitselect(a[7]^a[9],a[7],a[8]); a[8] = bitselect(a[8]^m5,a[8],a[9]); a[9] = bitselect(a[9]^m6,a[9],m5);\
            if (outsz > 8) { \
                m5 = a[10]; m6 = a[11]; a[10] = bitselect(a[10]^a[12],a[10],a[11]); a[11] = bitselect(a[11]^a[13],a[11],a[12]); a[12] = bitselect(a[12]^a[14],a[12],a[13]); a[13] = bitselect(a[13]^m5,a[13],a[14]); a[14] = bitselect(a[14]^m6,a[14],m5);\
                m5 = a[15]; m6 = a[16]; a[15] = bitselect(a[15]^a[17],a[15],a[16]); a[16] = bitselect(a[16]^a[18],a[16],a[17]); a[17] = bitselect(a[17]^a[19],a[17],a[18]); a[18] = bitselect(a[18]^m5,a[18],a[19]); a[19] = bitselect(a[19]^m6,a[19],m5);\
                m5 = a[20]; m6 = a[21]; a[20] = bitselect(a[20]^a[22],a[20],a[21]); a[21] = bitselect(a[21]^a[23],a[21],a[22]); a[22] = bitselect(a[22]^a[24],a[22],a[23]); a[23] = bitselect(a[23]^m5,a[23],a[24]); a[24] = bitselect(a[24]^m6,a[24],m5);\
            } \
        } \
    } \
 } while(0)


#define KECCAK_PROCESS(st, in_size, out_size)    do { \
    for (int r = 0; r < 24; ++r) { \
        int os = (r < 23 ? 25 : (out_size));\
        KECCAKF_1600_RND(st, r, os); \
    } \
} while(0)

#define fnv(x, y)        ((x) * FNV_PRIME ^ (y))
#define fnv_reduce(v)    fnv(fnv(fnv(v.x, v.y), v.z), v.w)

static void SHA3_512(uint2 *s) {
	uint2 st[25];

	for (uint i = 0; i < 8; ++i) {
		st[i] = s[i];
	}

	st[8] = (uint2)(0x00000001, 0x80000000);

	for (uint i = 9; i != 25; ++i) {
		st[i] = (uint2)(0);
	}
	
	KECCAK_PROCESS(st, 8, 8);
	
	for (uint i = 0; i < 8; ++i) {
		s[i] = st[i];
	}
}

typedef union _Node {
    uint dwords[16];
    uint2 qwords[8];
    uint4 dqwords[4];
} Node;



__attribute__((reqd_work_group_size(WORKSIZE , 1, 1)))
__kernel void build (__global Node * DAG, __global const uint16 *_Cache, uint dag_size, uint light_size, uint start) {
    __global const Node *Cache = (__global const Node *) _Cache;
    const uint gid = get_global_id(0);
    uint NodeIdx = start + gid;
        
    const uint thread_id = gid & 3;

    __local Node sharebuf[WORKSIZE];
    __local uint indexbuf[WORKSIZE];
    __local Node *dagNode = sharebuf + (get_local_id(0) / 4) * 4;
    __local uint *indexes = indexbuf + (get_local_id(0) / 4) * 4;
    __global const Node *parentNode;

    Node DAGNode = Cache[NodeIdx % light_size];

    DAGNode.dwords[0] ^= NodeIdx;
    SHA3_512(DAGNode.qwords);

    dagNode[thread_id] = DAGNode;
    
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint i = 0; i < 512; ++i) {
        uint ParentIdx = fnv(NodeIdx ^ i, dagNode[thread_id].dwords[i & 15]) % light_size;
        indexes[thread_id] = ParentIdx;
        barrier(CLK_LOCAL_MEM_FENCE);

        for (uint t = 0; t < 4; ++t) {
            uint parentIndex = indexes[t];
            parentNode = Cache + parentIndex;

            dagNode[t].dqwords[thread_id] = fnv(dagNode[t].dqwords[thread_id], parentNode->dqwords[thread_id]);
            barrier(CLK_LOCAL_MEM_FENCE);
        }
    }
    
    DAGNode = dagNode[thread_id];

    SHA3_512(DAGNode.qwords);
    
    // Prevent to write behind the dag end
    if (NodeIdx < 2*dag_size) DAG[NodeIdx] = DAGNode;
}



/****************************************************

	 Blake3 for the FishHash main kernel

      Mostly a stripped down code from lolMiner

****************************************************/



#define IV_0 0x6A09E667UL
#define IV_1 0xBB67AE85UL
#define IV_2 0x3C6EF372UL
#define IV_3 0xA54FF53AUL
#define IV_4 0x510E527FUL
#define IV_5 0x9B05688CUL
#define IV_6 0x1F83D9ABUL
#define IV_7 0x5BE0CD19UL

#define CHUNK_START (1 << 0)
#define CHUNK_END (1 << 1)
#define ROOT (1 << 3)

#define BLAKE3_BLOCK_LEN 64


// Note: OpenCL rotate is a left rotate 
#define gFunc(va, vb, vc, vd, x, y)		\
    do                      			\
    {                            		\
        va = va + vb + x;        		\
        vd = rotate(vd ^ va, 16U);		\
        vc = vc + vd;             		\
        vb = rotate(vb ^ vc, 20U); 		\
        va = va + vb + y;         		\
        vd = rotate(vd ^ va, 24U);  		\
        vc = vc + vd;             		\
        vb = rotate(vb ^ vc, 25U);  		\
    } while (0)
    
    
    
#define BLAKE_CORE() {					\
	gFunc(v.s0, v.s4, v.s8, v.sc, m.s0, m.s1);	\
	gFunc(v.s1, v.s5, v.s9, v.sd, m.s2, m.s3);	\
	gFunc(v.s2, v.s6, v.sa, v.se, m.s4, m.s5);	\
	gFunc(v.s3, v.s7, v.sb, v.sf, m.s6, m.s7);	\
	gFunc(v.s0, v.s5, v.sa, v.sf, m.s8, m.s9);	\
	gFunc(v.s1, v.s6, v.sb, v.sc, m.sa, m.sb);	\
	gFunc(v.s2, v.s7, v.s8, v.sd, m.sc, m.sd);	\
	gFunc(v.s3, v.s4, v.s9, v.se, m.se, m.sf);	\
							\
	gFunc(v.s0, v.s4, v.s8, v.sc, m.s2, m.s6);	\
	gFunc(v.s1, v.s5, v.s9, v.sd, m.s3, m.sa);	\
	gFunc(v.s2, v.s6, v.sa, v.se, m.s7, m.s0);	\
	gFunc(v.s3, v.s7, v.sb, v.sf, m.s4, m.sd);	\
	gFunc(v.s0, v.s5, v.sa, v.sf, m.s1, m.sb);	\
	gFunc(v.s1, v.s6, v.sb, v.sc, m.sc, m.s5);	\
	gFunc(v.s2, v.s7, v.s8, v.sd, m.s9, m.se);	\
	gFunc(v.s3, v.s4, v.s9, v.se, m.sf, m.s8);	\
							\
	gFunc(v.s0, v.s4, v.s8, v.sc, m.s3, m.s4);	\
	gFunc(v.s1, v.s5, v.s9, v.sd, m.sa, m.sc);	\
	gFunc(v.s2, v.s6, v.sa, v.se, m.sd, m.s2);	\
	gFunc(v.s3, v.s7, v.sb, v.sf, m.s7, m.se);	\
	gFunc(v.s0, v.s5, v.sa, v.sf, m.s6, m.s5);	\
	gFunc(v.s1, v.s6, v.sb, v.sc, m.s9, m.s0);	\
	gFunc(v.s2, v.s7, v.s8, v.sd, m.sb, m.sf);	\
	gFunc(v.s3, v.s4, v.s9, v.se, m.s8, m.s1);	\
							\
	gFunc(v.s0, v.s4, v.s8, v.sc, m.sa, m.s7);	\
	gFunc(v.s1, v.s5, v.s9, v.sd, m.sc, m.s9);	\
	gFunc(v.s2, v.s6, v.sa, v.se, m.se, m.s3);	\
	gFunc(v.s3, v.s7, v.sb, v.sf, m.sd, m.sf);	\
	gFunc(v.s0, v.s5, v.sa, v.sf, m.s4, m.s0);	\
	gFunc(v.s1, v.s6, v.sb, v.sc, m.sb, m.s2);	\
	gFunc(v.s2, v.s7, v.s8, v.sd, m.s5, m.s8);	\
	gFunc(v.s3, v.s4, v.s9, v.se, m.s1, m.s6);	\
							\
	gFunc(v.s0, v.s4, v.s8, v.sc, m.sc, m.sd);	\
	gFunc(v.s1, v.s5, v.s9, v.sd, m.s9, m.sb);	\
	gFunc(v.s2, v.s6, v.sa, v.se, m.sf, m.sa);	\
	gFunc(v.s3, v.s7, v.sb, v.sf, m.se, m.s8);	\
	gFunc(v.s0, v.s5, v.sa, v.sf, m.s7, m.s2);	\
	gFunc(v.s1, v.s6, v.sb, v.sc, m.s5, m.s3);	\
	gFunc(v.s2, v.s7, v.s8, v.sd, m.s0, m.s1);	\
	gFunc(v.s3, v.s4, v.s9, v.se, m.s6, m.s4);	\
							\
	gFunc(v.s0, v.s4, v.s8, v.sc, m.s9, m.se);	\
	gFunc(v.s1, v.s5, v.s9, v.sd, m.sb, m.s5);	\
	gFunc(v.s2, v.s6, v.sa, v.se, m.s8, m.sc);	\
	gFunc(v.s3, v.s7, v.sb, v.sf, m.sf, m.s1);	\
	gFunc(v.s0, v.s5, v.sa, v.sf, m.sd, m.s3);	\
	gFunc(v.s1, v.s6, v.sb, v.sc, m.s0, m.sa);	\
	gFunc(v.s2, v.s7, v.s8, v.sd, m.s2, m.s6);	\
	gFunc(v.s3, v.s4, v.s9, v.se, m.s4, m.s7);	\
							\
	gFunc(v.s0, v.s4, v.s8, v.sc, m.sb, m.sf);	\
	gFunc(v.s1, v.s5, v.s9, v.sd, m.s5, m.s0);	\
	gFunc(v.s2, v.s6, v.sa, v.se, m.s1, m.s9);	\
	gFunc(v.s3, v.s7, v.sb, v.sf, m.s8, m.s6);	\
	gFunc(v.s0, v.s5, v.sa, v.sf, m.se, m.sa);	\
	gFunc(v.s1, v.s6, v.sb, v.sc, m.s2, m.sc);	\
	gFunc(v.s2, v.s7, v.s8, v.sd, m.s3, m.s4);	\
	gFunc(v.s3, v.s4, v.s9, v.se, m.s7, m.sd);	\
}



/****************************************************

	   FishHash main search kernel

****************************************************/

typedef union {
    ulong8 ulong8s[1];
    ulong4 ulong4s[2];
    uint2 uint2s[8];
    uint4 uint4s[4];
    uint8 uint8s[2];
    uint16 uint16s[1];
    ulong ulongs[8];
    uint  uints[16];
} compute_hash_share;

__attribute__((reqd_work_group_size(WORKSIZE , 1, 1)))
__kernel void mine (	__global uint8 * dag,
			__global uint16 * blockHeader,
			__global ulong *  output,
			uint dagSize,
			ulong startNonce,
			ulong target) {
			
    const uint thread_id = get_local_id(0) % 4;
    const uint hash_id = get_local_id(0) / 4;
    const uint gid = get_global_id(0);
    
    ulong nonce = startNonce + gid;	
    // We need to flip the endianess of the nonce to be sure the extra nonce is in right position
    nonce = as_ulong(as_uchar8(nonce).s76543210);	

    __local compute_hash_share sharebuf[WORKSIZE / 4];
    __local uint buffer[WORKSIZE];
    __local compute_hash_share * const share = sharebuf + hash_id;

    // Each thread doing blake3 of one nonce here
    
    uint16 v;
    uint8 cv;
    uint16 m;
    
    v.s0 = IV_0;
    v.s1 = IV_1;
    v.s2 = IV_2;
    v.s3 = IV_3;
    v.s4 = IV_4;
    v.s5 = IV_5;
    v.s6 = IV_6;
    v.s7 = IV_7;
        
    // The first blake 3 needs 3 passes for the block header
    for (uint c=0; c<3; c++) {
    	cv = v.lo;
    	m = blockHeader[c];
    	
    	v.s8 = IV_0;
	v.s9 = IV_1;
	v.sa = IV_2;
	v.sb = IV_3;
	v.sc = 0;
	v.sd = 0;
	
	if (c == 0) {
	    v.se = (uint) 64;
	    v.sf = (uint) CHUNK_START;
	} else if (c == 1) {
	    v.se = (uint) 64;
	    v.sf = 0;
	} else if (c == 2) {
	    // Taking care of the new block header format	
	    m.sbc = as_uint2(nonce);	  
	    v.se = (uint) 52;
	    v.sf = (uint) CHUNK_END | ROOT;
	}
    	
    	BLAKE_CORE();
    	
    	// XOR the output for a total 64 byte output (of the last pass)
    	v.lo ^= v.hi;
    	v.hi ^= cv;
    }
    
    uint8 oldV = v.lo;
    
    // Entering the pointer chaseing phase, here 4 threads work together on one hash
    uint8 mix;
    uint8 mixResult;
        
    #pragma unroll 1
    for (uint tid = 0; tid < 4; tid++) {
            if (tid == thread_id) {
                share->uint2s[0] = v.s01;
                share->uint2s[1] = v.s23;
                share->uint2s[2] = v.s45;
                share->uint2s[3] = v.s67;
                share->uint2s[4] = v.s89;
                share->uint2s[5] = v.sab;
                share->uint2s[6] = v.scd;
                share->uint2s[7] = v.sef;
            }
            
            barrier(CLK_LOCAL_MEM_FENCE);

            mix = share->uint8s[thread_id & 1];
                        
            // 32 rounds
            for (uint a = 0; a < 32; a++) {
                       
            	barrier(CLK_LOCAL_MEM_FENCE);
            
            	// Share mix.s0 and mix.s4 for the address calculation
            	share->uint2s[thread_id] = (uint2) (mix.s0, mix.s4);
            	
            	barrier(CLK_LOCAL_MEM_FENCE);
            	
            	// FishHash address calculation
            	uint p0 = share->uints[0] % dagSize;
		uint p1 = share->uints[1] % dagSize;
		uint p2 = share->uints[2] % dagSize;
				
		uint8 fetch0 = dag[4*p0 + thread_id];
		uint8 fetch1 = dag[4*p1 + thread_id];
		uint8 fetch2 = dag[4*p2 + thread_id];
				
		fetch1 = fnv(mix, fetch1);
		fetch2 = mix ^ fetch2;
		
		// Each thread doing 4 ulong calculations
		mix.s01 = as_uint2( as_ulong(fetch0.s01) * as_ulong(fetch1.s01) + as_ulong(fetch2.s01) );
            	mix.s23 = as_uint2( as_ulong(fetch0.s23) * as_ulong(fetch1.s23) + as_ulong(fetch2.s23) );
            	mix.s45 = as_uint2( as_ulong(fetch0.s45) * as_ulong(fetch1.s45) + as_ulong(fetch2.s45) );
            	mix.s67 = as_uint2( as_ulong(fetch0.s67) * as_ulong(fetch1.s67) + as_ulong(fetch2.s67) );
            	 
            }
            
            barrier(CLK_LOCAL_MEM_FENCE);
            
            share->uint2s[thread_id] = (uint2)(fnv_reduce(mix.lo), fnv_reduce(mix.hi));
            
            barrier(CLK_LOCAL_MEM_FENCE);
            
            if (tid == thread_id) {
                mixResult.s01 = share->uint2s[0];
                mixResult.s23 = share->uint2s[1];
                mixResult.s45 = share->uint2s[2];
                mixResult.s67 = share->uint2s[3];
            }
            
            barrier(CLK_LOCAL_MEM_FENCE);
    }
    
    // The old output is the new input
    m = v;
    
    v.s0 = IV_0;
    v.s1 = IV_1;
    v.s2 = IV_2;
    v.s3 = IV_3;
    v.s4 = IV_4;
    v.s5 = IV_5;
    v.s6 = IV_6;
    v.s7 = IV_7;
    
    // The second blake 3 needs 2 passes for the final hash   
    for (uint c=0; c<2; c++) {
    	v.s8 = IV_0;
	v.s9 = IV_1;
	v.sa = IV_2;
	v.sb = IV_3;
	v.sc = 0;
	v.sd = 0;
	
	if (c == 0) {
	    v.se = (uint) 64;
	    v.sf = (uint) CHUNK_START;	
	} else if (c == 1) {
	    m.lo = mixResult;
	    m.hi = (uint8) 0;
	    v.se = (uint) 32;
	    v.sf = (uint) CHUNK_END | ROOT;
	}
	
	BLAKE_CORE();
    	
    	// XOR the output for a total 32 byte output 
    	v.lo ^= v.hi;
    }
    
    ulong compare = as_ulong(as_uchar8(v.s01).s76543210);
    if (compare < target) {	     
    	uint index = atomic_inc( (__global uint *) &output[0]);
    	index &= 0x3;
    	
    	output[1+index] = nonce;
    } 
}			
