#include "WAVMWasmModule.h"
#include "syscalls.h"

#include <faabric/util/logging.h>
#include <faabric/util/bytes.h>
#include <wasm/chaining.h>
#include <wasm/ndp.h>

#include <cstdio>
#include <cstdint>
#include <algorithm>

#include <WAVM/Inline/FloatComponents.h>
#include <WAVM/IR/IR.h>
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

    BuiltinNdpPutArgs putArgs{};
    putArgs.key = std::vector(key, key+keyLen);
    putArgs.value = std::vector(data, data+dataLen);
    int put_call = makeChainedCall(BUILTIN_NDP_PUT_FUNCTION, 0, nullptr, putArgs.asBytes());
    int put_result = awaitChainedCall(put_call);

    return put_result;
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
    *outDataLen = 0;

    if (maxRequestedLen < 0) {
      maxRequestedLen = INT32_MAX;
    }

    logger->debug("S - ndpos_getMmap - {} {} {} {}", keyPtr, keyLen, maxRequestedLen, outDataLenPtr);

    BuiltinNdpGetArgs putArgs{};
    putArgs.key = std::vector(key, key+keyLen);
    putArgs.offset = 0;
    putArgs.uptoBytes = maxRequestedLen;
    int put_call = makeChainedCall(BUILTIN_NDP_PUT_FUNCTION, 0, nullptr, putArgs.asBytes());
    auto put_result = awaitChainedCallMessage(put_call);

    if (put_result.returnvalue() != 0) {
      std::vector<uint8_t> outputData = faabric::util::stringToBytes(put_result.outputdata());
      size_t copyLen = (outputData.size() > maxRequestedLen) ? outputData.size() : maxRequestedLen;
      int growPages = (copyLen + WAVM::IR::numBytesPerPage - 1) / WAVM::IR::numBytesPerPage;
      Uptr oldPages{};
      if(Runtime::growMemory(memoryPtr, growPages, &oldPages) != Runtime::GrowResult::success) {
        logger->error("Error growing memory for faasm ndp getMmap by {} pages", growPages);
      }
      Uptr oldPagesEnd{oldPages * WAVM::IR::numBytesPerPage};
      U8* outDataPtr = Runtime::memoryArrayPtr<U8>(memoryPtr, oldPagesEnd, (Uptr)copyLen);
      std::copy_n(outputData.begin(), copyLen, outDataPtr);
      *outDataLen = copyLen;
    }

    return put_result.returnvalue();
}

void ndpLink() {}

}
