#pragma once

#include <cstdint>
#include <string>
#include <sstream>
#include <vector>

#include <cereal/types/vector.hpp>
#include <cereal/archives/portable_binary.hpp>

namespace wasm {

static const char* const BUILTIN_NDP_PUT_FUNCTION = "!builtin_storage_put";
static const char* const BUILTIN_NDP_GET_FUNCTION = "!builtin_storage_get";

class NDPBuiltinModule;

struct BuiltinFunction {
    const char *name;
    int (*function)(NDPBuiltinModule&, faabric::Message&);
};

extern const std::array<BuiltinFunction, 2> NDP_BUILTINS;

inline const BuiltinFunction& getNdpBuiltin(const std::string& functionName) {
    auto fn = std::find_if(NDP_BUILTINS.cbegin(), NDP_BUILTINS.cend(), [&](const BuiltinFunction& f){return f.name == functionName;});
    if (fn == NDP_BUILTINS.cend()) {
        throw std::runtime_error(std::string("Invalid builtin name: ") + functionName);
    }
    return *fn;
}

struct BuiltinNdpPutArgs {
    std::vector<uint8_t> key;
    std::vector<uint8_t> value;

    template<class Archive>
    void serialize(Archive& ar)
    {
        ar(key, value);
    }

    inline std::vector<uint8_t> asBytes() const {
        std::stringstream out;
        cereal::PortableBinaryOutputArchive ar(out);
        ar(*this);
        std::string outStr = out.str();
        return std::vector<uint8_t>(outStr.begin(), outStr.end());
    }

    inline static BuiltinNdpPutArgs fromBytes(const std::vector<uint8_t>& bytes) {
        std::string inStr(bytes.begin(), bytes.end());
        std::stringstream in(inStr);
        cereal::PortableBinaryInputArchive ar(in);
        BuiltinNdpPutArgs out;
        ar(out);
        return out;
    }
};

struct BuiltinNdpGetArgs {
    std::vector<uint8_t> key;
    uint64_t offset, uptoBytes;

    template<class Archive>
    void serialize(Archive& ar)
    {
        ar(key, offset, uptoBytes);
    }

    inline std::vector<uint8_t> asBytes() const {
        std::stringstream out;
        cereal::PortableBinaryOutputArchive ar(out);
        ar(*this);
        std::string outStr = out.str();
        return std::vector<uint8_t>(outStr.begin(), outStr.end());
    }

    inline static BuiltinNdpGetArgs fromBytes(const std::vector<uint8_t>& bytes) {
        std::string inStr(bytes.begin(), bytes.end());
        std::stringstream in(inStr);
        cereal::PortableBinaryInputArchive ar(in);
        BuiltinNdpGetArgs out;
        ar(out);
        return out;
    }
};

}
