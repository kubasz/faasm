#include "WasmModule.h"
#include "wasm/chaining.h"

#include <faabric/scheduler/Scheduler.h>
#include <faabric/util/bytes.h>

namespace wasm {
int awaitChainedCall(unsigned int messageId)
{
    int callTimeoutMs = faabric::util::getSystemConfig().chainedCallTimeout;

    int returnCode = 1;
    try {
        faabric::scheduler::Scheduler& sch = faabric::scheduler::getScheduler();
        const faabric::Message result =
          sch.getFunctionResult(messageId, callTimeoutMs);
        returnCode = result.returnvalue();
    } catch (faabric::redis::RedisNoResponseException& ex) {
        faabric::util::getLogger()->error(
          "Timed out waiting for chained call: {}", messageId);
    } catch (std::exception& ex) {
        faabric::util::getLogger()->error(
          "Non-timeout exception waiting for chained call: {}", ex.what());
    }

    return returnCode;
}

int makeChainedCall(const std::string& functionName,
                    int wasmFuncPtr,
                    const char* pyFuncName,
                    const std::vector<uint8_t>& inputData,
                    bool isStorage)
{
    faabric::scheduler::Scheduler& sch = faabric::scheduler::getScheduler();
    faabric::Message* originalCall = getExecutingCall();

    // Chained calls should be asynchronous as we will wait for the result on
    // the message queue
    faabric::Message call =
      faabric::util::messageFactory(originalCall->user(), functionName);
    call.set_inputdata(inputData.data(), inputData.size());
    call.set_funcptr(wasmFuncPtr);
    call.set_isasync(true);
    call.set_isstorage(isStorage);

    call.set_pythonuser(originalCall->pythonuser());
    call.set_pythonfunction(originalCall->pythonfunction());
    if (pyFuncName != nullptr) {
        call.set_pythonentry(pyFuncName);
    }
    call.set_ispython(originalCall->ispython());

    const std::string origStr =
      faabric::util::funcToString(*originalCall, false);
    const std::string chainedStr = faabric::util::funcToString(call, false);

    faabric::util::SystemConfig& conf = faabric::util::getSystemConfig();
    sch.callFunction(call);
    faabric::util::getLogger()->debug("Chained {} ({}) -> {} ({})",
                                      origStr,
                                      conf.endpointHost,
                                      chainedStr,
                                      call.scheduledhost());

    sch.logChainedFunction(originalCall->id(), call.id());

    return call.id();
}

int spawnChainedThread(const std::string& snapshotKey,
                       size_t snapshotSize,
                       int funcPtr,
                       int argsPtr)
{
    faabric::scheduler::Scheduler& sch = faabric::scheduler::getScheduler();

    faabric::Message* originalCall = getExecutingCall();
    faabric::Message call = faabric::util::messageFactory(
      originalCall->user(), originalCall->function());
    call.set_isasync(true);

    // Snapshot details
    call.set_snapshotkey(snapshotKey);
    call.set_snapshotsize(snapshotSize);

    // Function pointer and args
    // NOTE - with a pthread interface we only ever pass the function a single
    // pointer argument, hence we use the input data here to hold this argument
    // as a string
    call.set_funcptr(funcPtr);
    call.set_inputdata(std::to_string(argsPtr));

    const std::string origStr =
      faabric::util::funcToString(*originalCall, false);
    const std::string chainedStr = faabric::util::funcToString(call, false);

    // Schedule the call
    sch.callFunction(call);
    faabric::util::getLogger()->debug(
      "Chained thread {} ({}) -> {} {}({}) ({})",
      origStr,
      faabric::util::getSystemConfig().endpointHost,
      chainedStr,
      funcPtr,
      argsPtr,
      call.scheduledhost());

    return call.id();
}

int awaitChainedCallOutput(unsigned int messageId,
                           uint8_t* buffer,
                           int bufferLen)
{
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();
    int callTimeoutMs = faabric::util::getSystemConfig().chainedCallTimeout;

    faabric::scheduler::Scheduler& sch = faabric::scheduler::getScheduler();
    const faabric::Message result =
      sch.getFunctionResult(messageId, callTimeoutMs);

    if (result.type() == faabric::Message_MessageType_EMPTY) {
        logger->error("Cannot find output for {}", messageId);
    }

    std::vector<uint8_t> outputData =
      faabric::util::stringToBytes(result.outputdata());
    int outputLen =
      faabric::util::safeCopyToBuffer(outputData, buffer, bufferLen);

    if (outputLen < outputData.size()) {
        logger->warn(
          "Undersized output buffer: {} for {} output", bufferLen, outputLen);
    }

    return result.returnvalue();
}

faabric::Message awaitChainedCallMessage(unsigned int messageId)
{
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();
    int callTimeoutMs = faabric::util::getSystemConfig().chainedCallTimeout;

    faabric::scheduler::Scheduler& sch = faabric::scheduler::getScheduler();
    const faabric::Message result =
      sch.getFunctionResult(messageId, callTimeoutMs);

    if (result.type() == faabric::Message_MessageType_EMPTY) {
        logger->error("Cannot find output for {}", messageId);
    }

    return result;
}

int chainNdpCall(const std::string& snapshotKey,
                       size_t snapshotSize,
                 const std::string& inputData,
                       int funcPtr)
{
    faabric::scheduler::Scheduler& sch = faabric::scheduler::getScheduler();

    faabric::Message* originalCall = getExecutingCall();
    faabric::Message call = faabric::util::messageFactory(
      originalCall->user(), originalCall->function());
    call.set_isasync(true);

    // Snapshot details
    call.set_snapshotkey(snapshotKey);
    call.set_snapshotsize(snapshotSize);

    // Function pointer and args
    call.set_funcptr(funcPtr);
    call.set_inputdata(inputData);
    call.set_isstorage(true);
    call.set_isoutputmemorydelta(true);

    const std::string origStr =
      faabric::util::funcToString(*originalCall, false);
    const std::string chainedStr = faabric::util::funcToString(call, false);

    // Schedule the call
    sch.callFunction(call);
    faabric::util::getLogger()->debug(
      "Chained NDP call {} ({}) -> {} {}() ({})",
      origStr,
      faabric::util::getSystemConfig().endpointHost,
      chainedStr,
      funcPtr,
      call.scheduledhost());

    return call.id();
}

}
