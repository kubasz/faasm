#ifndef NDPAPI_H_INCLUDED
#define NDPAPI_H_INCLUDED 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t __faasmndp_put(const uint8_t* keyPtr, uint32_t keyLen, const uint8_t* dataPtr, uint32_t dataLen);

uint8_t* __faasmndp_getMmap(const uint8_t* keyPtr, uint32_t keyLen, uint32_t maxRequestedLen, uint32_t* outDataLenPtr);


#ifdef __cplusplus
}
#endif

#endif