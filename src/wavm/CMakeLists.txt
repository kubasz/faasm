include_directories(
        ${FAASM_INCLUDE_DIR}/wavm
        ${FAASM_WAVM_SOURCE_DIR}/Lib
)

set(HEADERS
        "${FAASM_INCLUDE_DIR}/wavm/OMPThreadPool.h"
        "${FAASM_INCLUDE_DIR}/wavm/WAVMWasmModule.h"
)

add_subdirectory(openmp)

set(LIB_FILES
        WAVMWasmModule.cpp
        LoadedDynamicModule.cpp
        syscalls.h
        chaining.cpp
        codegen.cpp
        dynlink.cpp
        env.cpp
        faasm.cpp
        io.cpp
        libcxx.cpp
        maths.cpp
        memory.cpp
        messages.cpp
        mpi.cpp
        ndp_objects.cpp
        network.cpp
        openmp.cpp
        OMPThreadPool.cpp
        process.cpp
        scheduling.cpp
        signals.cpp
        syscalls.cpp
        threads.cpp
        timing.cpp
        util.cpp
        ${HEADERS}
        )

faasm_private_lib(wavmmodule "${LIB_FILES}")
add_dependencies(wavmmodule cereal_ext)
target_link_libraries(wavmmodule PUBLIC wasm ir_cache libWAVM zstd::libzstd_static)
