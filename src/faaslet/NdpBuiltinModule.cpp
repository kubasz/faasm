#include <faaslet/NdpBuiltinModule.h>

#include <boost/filesystem.hpp>
#include <cereal/archives/binary.hpp>
#include <algorithm>
#include <stdexcept>
#include <array>
#include <sys/mman.h>
#include <sys/types.h>

#include <faabric/util/bytes.h>
#include <faabric/util/config.h>
#include <faabric/util/files.h>
#include <faabric/util/func.h>
#include <faabric/util/locks.h>
#include <faabric/util/memory.h>
#include <faabric/util/timing.h>
#include <wasm/ndp.h>
#include <wasm/serialisation.h>

namespace fs = boost::filesystem;

namespace wasm {

static fs::path userObjectDirPath(const std::string& user) {
    faabric::util::SystemConfig& conf = faabric::util::getSystemConfig();
    fs::path p(conf.sharedFilesStorageDir);
    p /= "objstore";
    p /= user;
    fs::create_directories(p);
    return p;
}

static std::string userObjectPath(const std::string& user, const std::vector<uint8_t>& key) {
    std::string filename = faabric::util::bytesToString(key);
    return (userObjectDirPath(user) / filename).string();
}

static int ndpGet(NDPBuiltinModule& module, faabric::Message& msg) {
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();
    auto args = BuiltinNdpGetArgs::fromBytes(faabric::util::stringToBytes(msg.inputdata()));
    std::string objPath = userObjectPath(msg.user(), args.key);
    if (!fs::exists(objPath)) {
        logger->debug("NDP GET file {} doesn't exist", objPath);
        return 1;
    }
    auto bytes = faabric::util::readFileToBytes(objPath);
    size_t startOffset = std::min(bytes.size(), args.offset);
    size_t dataLen = std::min(bytes.size() - startOffset, args.uptoBytes);
    logger->debug("NDP GET {} bytes starting at offset {} from file {} [bytes={}, args.offset={}, uptoBytes={}]",
        dataLen, startOffset, objPath, bytes.size(), args.offset, args.uptoBytes);
    msg.set_outputdata(static_cast<void*>(bytes.data() + startOffset), startOffset + dataLen);
    return 0;
}

static int ndpPut(NDPBuiltinModule& module, faabric::Message& msg) {
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();
    auto args = BuiltinNdpPutArgs::fromBytes(faabric::util::stringToBytes(msg.inputdata()));
    std::string objPath = userObjectPath(msg.user(), args.key);
    logger->debug("NDP PUT {} bytes to file {}", args.value.size(), objPath);
    faabric::util::writeBytesToFile(objPath, args.value);
    return 0;
}

const std::array<BuiltinFunction, 2> NDP_BUILTINS{{
    {BUILTIN_NDP_GET_FUNCTION, &ndpGet},
    {BUILTIN_NDP_PUT_FUNCTION, &ndpPut}
}};

NDPBuiltinModule::NDPBuiltinModule() :
    _isBound(false),
    boundFn(nullptr),
    input(),
    output()
{
}

NDPBuiltinModule::~NDPBuiltinModule()
{
    tearDown();
}

bool NDPBuiltinModule::tearDown()
{
    this->_isBound = false;
    this->boundFunction.clear();
    this->boundUser.clear();
    this->input.clear();
    this->output.clear();
    return true;
}

bool NDPBuiltinModule::isBound()
{
    return _isBound;
}

void NDPBuiltinModule::bindToFunction(const faabric::Message& msg)
{
    // TODO: builtins, throw std::runtime_error when not found
    std::string functionName = msg.function();
    _isBound = true;
    boundUser = msg.user();
    boundFunction = functionName;
    boundFn = &getNdpBuiltin(functionName);
}

void NDPBuiltinModule::bindToFunctionNoZygote(const faabric::Message& msg)
{
    this->bindToFunction(msg);
}

/**
 * Executes the given function call
 */
bool NDPBuiltinModule::execute(faabric::Message& msg, bool forceNoop)
{
    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();

    if (!_isBound) {
        throw std::runtime_error(
          "NDPBuiltinModule must be bound before executing function");
    } else {
        if (boundUser != msg.user() || boundFunction != msg.function()) {
            const std::string funcStr = faabric::util::funcToString(msg, true);
            logger->error("Cannot execute {} on builtin module bound to {}/{}",
                          funcStr,
                          boundUser,
                          boundFunction);
            throw std::runtime_error(
              "Cannot execute function on builtin module bound to another");
        }
    }

    setExecutingCall(&msg);

    // Call the function
    int returnValue = 0;
    bool success = true;
    logger->debug("Builtin execute {}", boundFunction);
    if (!forceNoop) {
        try {
            if (this->boundFn == nullptr) {
                throw std::runtime_error("Builtin bound function definition is null");
            }
            if (this->boundFn->function == nullptr) {
                throw std::runtime_error("Builtin bound function pointer is null");
            }
            returnValue = this->boundFn->function(*this, msg);
        } catch (...) {
            logger->error("Exception caught executing a builtin");
            success = false;
            returnValue = 1;
        }
    }

    // Record the return value
    msg.set_returnvalue(returnValue);

    return success;
}

uint32_t NDPBuiltinModule::mmapFile(uint32_t fd, uint32_t length)
{
    return 0;
}

uint32_t NDPBuiltinModule::mmapMemory(uint32_t length)
{
    return 0;
}

uint32_t NDPBuiltinModule::mmapPages(uint32_t pages)
{
    return 0;
}

uint8_t* NDPBuiltinModule::wasmPointerToNative(int32_t wasmPtr)
{
    return nullptr;
}

void NDPBuiltinModule::doSnapshot(std::ostream& outStream)
{
    cereal::BinaryOutputArchive archive(outStream);

    archive(input, output);
}

void NDPBuiltinModule::doRestore(std::istream& inStream)
{
    cereal::BinaryInputArchive archive(inStream);

    archive(input, output);
}

void NDPBuiltinModule::printDebugInfo()
{
    printf("\n------ Builtin Module debug info ------\n");

    if (isBound()) {
        printf("Bound user:         %s\n", boundUser.c_str());
        printf("Bound function:     %s\n", boundFunction.c_str());
        printf("Input length:       %lu\n", (unsigned long)input.size());
        printf("Output length:      %lu\n", (unsigned long)output.size());

        filesystem.printDebugInfo();
    } else {
        printf("Unbound\n");
    }

    printf("-------------------------------\n");

    fflush(stdout);
}

}
