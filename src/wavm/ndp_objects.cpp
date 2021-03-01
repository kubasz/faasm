#include "WAVMWasmModule.h"
#include "syscalls.h"

#include <faabric/util/logging.h>
#include <faabric/util/bytes.h>
#include <faabric/util/files.h>
#include <wasm/chaining.h>
#include <wasm/ndp.h>
#include <faaslet/NdpBuiltinModule.h>

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
    const faabric::util::SystemConfig& config = faabric::util::getSystemConfig();

    auto module_ = getExecutingWAVMModule();
    Runtime::Memory* memoryPtr = module_->defaultMemory;
    U8* key = Runtime::memoryArrayPtr<U8>(memoryPtr, (Uptr)keyPtr, (Uptr)keyLen);
    U8* data =
      Runtime::memoryArrayPtr<U8>(memoryPtr, (Uptr)dataPtr, (Uptr)dataLen);

    logger->debug("S - ndpos_put - {} {} {} {}", keyPtr, keyLen, dataPtr, dataLen);

    BuiltinNdpPutArgs putArgs{};
    putArgs.key = std::vector(key, key+keyLen);
    putArgs.value = std::vector(data, data+dataLen);
    faabric::Message put_result;
    if (config.isStorageNode) {
        faabric::Message msg = faabric::util::messageFactory(module_->getBoundUser(), BUILTIN_NDP_PUT_FUNCTION);
        auto bytes = putArgs.asBytes();
        msg.set_inputdata(bytes.data(), bytes.size());
        msg.set_isstorage(true);
        NDPBuiltinModule mod{};
        mod.bindToFunction(msg);
        mod.execute(msg);
        put_result = msg;
    } else {
        int put_call = makeChainedCall(BUILTIN_NDP_PUT_FUNCTION, 0, nullptr, putArgs.asBytes(), true);
        awaitChainedCallMessage(put_call);
    }

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
    const faabric::util::SystemConfig& config = faabric::util::getSystemConfig();

    auto call_ = getExecutingCall();
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
    faabric::Message get_result;
    if (config.isStorageNode) {
        faabric::Message msg = faabric::util::messageFactory(module_->getBoundUser(), BUILTIN_NDP_GET_FUNCTION);
        auto bytes = getArgs.asBytes();
        msg.set_inputdata(bytes.data(), bytes.size());
        msg.set_isstorage(true);
        NDPBuiltinModule mod{};
        mod.bindToFunction(msg);
        mod.execute(msg);
        get_result = msg;
        setExecutingCall(call_);
        setExecutingModule(module_);
    } else {
        int get_call = makeChainedCall(BUILTIN_NDP_GET_FUNCTION, 0, nullptr, getArgs.asBytes(), true);
        get_result = awaitChainedCallMessage(get_call);
    }

    logger->debug("SB - ndpos_getMmap builtin result: ret {}, {} out bytes",
        get_result.returnvalue(), get_result.outputdata().size());

    U32 outPtr{0};
    if (get_result.returnvalue() == 0) {
        std::vector<uint8_t> outputData = faabric::util::stringToBytes(get_result.outputdata());
        size_t copyLen = std::min(outputData.size(), (size_t)maxRequestedLen);
        U32 oldPagesEnd = module_->mmapMemory(copyLen);
        U32 oldPageNumberEnd = oldPagesEnd / WASM_BYTES_PER_PAGE;
        U32 newPageNumberLen = Runtime::getMemoryNumPages(memoryPtr) - oldPageNumberEnd;
        logger->debug("ndpos_getMmap data start at addr {:08x} page {} len {}", oldPagesEnd, oldPageNumberEnd, newPageNumberLen);
        U8* outDataPtr = Runtime::memoryArrayPtr<U8>(memoryPtr, oldPagesEnd, (Uptr)copyLen);
        std::copy_n(outputData.begin(), copyLen, outDataPtr);
        *outDataLen = copyLen;
        outPtr = oldPagesEnd;
        module_->ndpMappedPtrLens.push_back(std::make_pair(oldPageNumberEnd, newPageNumberLen));
    }

    return outPtr;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
                               "__faasmndp_storageCallAndAwait",
                               I32,
                               __faasmndp_storageCallAndAwait,
                               I32 wasmFuncPtr)
{
    auto logger = faabric::util::getLogger();
    const faabric::util::SystemConfig& config = faabric::util::getSystemConfig();
    logger->debug(
        "S - __faasmndp_storageCallAndAwait - {} {} {}", wasmFuncPtr);

    faabric::Message* call = getExecutingCall();

    WAVMWasmModule* thisModule = getExecutingWAVMModule();

    if(config.isStorageNode) {
        auto funcInstance = thisModule->getFunctionFromPtr(wasmFuncPtr);
        auto funcType = Runtime::getFunctionType(funcInstance);
        if (funcType.results().size() != 1 || funcType.params().size() != 0) {
            throw std::invalid_argument("Wrong function signature for storageCallAndAwait");
        }
        std::vector<IR::UntaggedValue> funcArgs = {0};
        IR::UntaggedValue result;
        Runtime::invokeFunction(thisModule->executionContext, funcInstance, funcType, funcArgs.data(), &result);
        return result.i32;
    } else {
        int myCallId = getExecutingCall()->id();
        std::string snapshotKey = std::string("faasmndp_snapshot_") + std::to_string(myCallId);
        size_t snapshotSize = thisModule->snapshotToState(snapshotKey);

        int ndpCallId = chainNdpCall(snapshotKey, snapshotSize, call->inputdata(), wasmFuncPtr);
        faabric::Message ndpResult = awaitChainedCallMessage(ndpCallId);

        if (ndpResult.returnvalue() != 0) {
            call->set_outputdata(ndpResult.outputdata());
            logger->debug("Chained NDP resulted in error code {}", ndpResult.returnvalue());
            return ndpResult.returnvalue();
        }

        logger->debug("NDP delta restore from {} bytes", ndpResult.outputdata().size());
        faabric::util::writeBytesToFile("/usr/local/faasm/debug_shared_store/debug_delta.bin", faabric::util::stringToBytes(ndpResult.outputdata()));
        std::istringstream memoryDelta(ndpResult.outputdata());
        thisModule->deltaRestore(memoryDelta);
    }

    return 0;
}

void ndpLink() {}

}
