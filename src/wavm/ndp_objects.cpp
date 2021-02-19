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
    int put_call = makeChainedCall(BUILTIN_NDP_PUT_FUNCTION, 0, nullptr, putArgs.asBytes(), true);
    auto put_result = awaitChainedCallMessage(put_call);
    logger->debug("SB - ndpos_put builtin result: ret {}, {} out bytes",
        put_result.returnvalue(), put_result.outputdata().size());

    return put_result.returnvalue();
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
                               "__faasmndp_getMmap",
                               I32,
                               __faasmndp_getMmap,
                               I32 keyPtr,
                               I32 keyLen,
                               U32 maxRequestedLen,
                               I32 outDataLenPtr)
{
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();

    auto module_ = getExecutingWAVMModule();
    Runtime::Memory* memoryPtr = module_->defaultMemory;
    U8* key = Runtime::memoryArrayPtr<U8>(memoryPtr, (Uptr)keyPtr, (Uptr)keyLen);
    U32* outDataLen = &Runtime::memoryRef<U32>(memoryPtr, (Uptr)outDataLenPtr);
    *outDataLen = 0;

    logger->debug("S - ndpos_getMmap - {} {} {:x} {}", keyPtr, keyLen, maxRequestedLen, outDataLenPtr);

    BuiltinNdpGetArgs getArgs{};
    getArgs.key = std::vector(key, key+keyLen);
    getArgs.offset = 0;
    getArgs.uptoBytes = maxRequestedLen;
    int get_call = makeChainedCall(BUILTIN_NDP_GET_FUNCTION, 0, nullptr, getArgs.asBytes(), true);
    auto get_result = awaitChainedCallMessage(get_call);

    logger->debug("SB - ndpos_getMmap builtin result: ret {}, {} out bytes",
        get_result.returnvalue(), get_result.outputdata().size());

    U32 outPtr{0};
    if (get_result.returnvalue() == 0) {
        std::vector<uint8_t> outputData = faabric::util::stringToBytes(get_result.outputdata());
        size_t copyLen = std::min(outputData.size(), (size_t)maxRequestedLen);
        U32 oldPagesEnd = module_->mmapMemory(copyLen);
        logger->debug("ndpos_getMmap data start at addr {:08x}", oldPagesEnd);
        U8* outDataPtr = Runtime::memoryArrayPtr<U8>(memoryPtr, oldPagesEnd, (Uptr)copyLen);
        std::copy_n(outputData.begin(), copyLen, outDataPtr);
        *outDataLen = copyLen;
        outPtr = oldPagesEnd;
    }

    return outPtr;
}

void ndpLink() {}

}
