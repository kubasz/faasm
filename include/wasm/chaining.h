#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <faabric/proto/faabric.pb.h>

namespace wasm {

int awaitChainedCall(unsigned int messageId);

int awaitChainedCallOutput(unsigned int messageId,
                           uint8_t* buffer,
                           int bufferLen);

int makeChainedCall(const std::string& functionName,
                    int wasmFuncPtr,
                    const char* pyFunc,
                    const std::vector<uint8_t>& inputData,
                    bool isStorage = false);

faabric::Message awaitChainedCallMessage(unsigned int messageId);

int chainNdpCall(const std::string& snapshotKey,
                       size_t snapshotSize,
                 const std::string& inputData,
                       int funcPtr);

}
