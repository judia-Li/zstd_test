#ifndef __ZSTD_OPENCL_MGR_H__
#define __ZSTD_OPENCL_MGR_H__

#define ZSTD_OPENCL_OK (0)
#define ZSTD_OPENCL_NG (-1)

#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

int createOpenCLComponents(void);
void releaseOpenCLComponents(void);
void ZSTD_fillDoubleHashTableCL(size_t ipSize, const BYTE* ip, size_t smHashIndexArraySize, U32* smHashIndexArray, size_t lgHashIndexArraySize, U32* lgHashIndexArray);

#ifdef __cplusplus
}
#endif

#endif // __ZSTD_OPENCL_MGR_H__
