#include "WAVMWasmModule.h"
#include "syscalls.h"

#include <faabric/util/logging.h>

#include <stdio.h>

#include <WAVM/Inline/FloatComponents.h>
#include <WAVM/Runtime/Intrinsics.h>
#include <WAVM/Runtime/Runtime.h>

using namespace WAVM;

namespace wasm {

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
                               "__faasmndp_put",
                               I32,
                               __faasmndp_put,
                               I32 keyPtr,
                               I32 keyLen,
                               I32 dataPtr,
                               I32 dataLen)
{
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();

    Runtime::Memory* memoryPtr = getExecutingWAVMModule()->defaultMemory;
    U8* key = Runtime::memoryArrayPtr<U8>(memoryPtr, (Uptr)keyPtr, (Uptr)keyLen);
    U8* data =
      Runtime::memoryArrayPtr<U8>(memoryPtr, (Uptr)dataPtr, (Uptr)dataLen);

    logger->debug("S - ndpos_put - {} {} {} {}", keyPtr, keyLen, dataPtr, dataLen);

    return 0;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
                               "__faasmndp_getMmap",
                               I32,
                               __faasmndp_getMmap,
                               I32 keyPtr,
                               I32 keyLen,
                               I32 maxRequestedLen,
                               I32 outDataLenPtr)
{
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();

    Runtime::Memory* memoryPtr = getExecutingWAVMModule()->defaultMemory;
    U8* key = Runtime::memoryArrayPtr<U8>(memoryPtr, (Uptr)keyPtr, (Uptr)keyLen);
    U32* outDataLen = &Runtime::memoryRef<U32>(memoryPtr, (Uptr)outDataLenPtr);

    logger->debug("S - ndpos_getMmap - {} {} {} {}", keyPtr, keyLen, maxRequestedLen, outDataLenPtr);

    *outDataLen = 0;

    return 0;
}

void ndpLink() {}

}
