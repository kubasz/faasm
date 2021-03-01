#pragma once

#include <wasm/WasmModule.h>
#include <wasm/ndp.h>
#include <string>
#include <vector>
#include <functional>

namespace wasm {

class NDPBuiltinModule;

class NDPBuiltinModule final
  : public WasmModule
{
  public:
    NDPBuiltinModule();

    ~NDPBuiltinModule();

    // ----- Module lifecycle -----
    void bindToFunction(const faabric::Message& msg) override;

    void bindToFunctionNoZygote(const faabric::Message& msg) override;

    bool execute(faabric::Message& msg, bool forceNoop = false) override;

    bool isBound() override;

    bool tearDown();

    inline void flush() override {}

    // ----- Memory management -----
    uint32_t mmapMemory(uint32_t length) override;

    uint32_t mmapPages(uint32_t pages) override;

    uint32_t mmapFile(uint32_t fp, uint32_t length) override;

    uint8_t* wasmPointerToNative(int32_t wasmPtr) override;

    // ----- Environment variables
    inline void writeWasmEnvToMemory(uint32_t envPointers,
                              uint32_t envBuffer) override {}

    // ----- CoW memory -----
    inline void writeMemoryToFd(int fd) override {}

    inline void mapMemoryFromFd() override {}

    // ----- Debug -----
    void printDebugInfo() override;

    // ----- Execution -----
    inline void writeArgvToMemory(uint32_t wasmArgvPointers,
                           uint32_t wasmArgvBuffer) override {}

  protected:
    void doSnapshot(std::ostream& outStream) override;

    void doRestore(std::istream& inStream) override;

  private:
    bool _isBound = false;
    const BuiltinFunction* boundFn;
    std::vector<uint8_t> input, output;
};

}
