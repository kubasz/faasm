#include "faasm/faasm.h"

#include "ndpapi.h"

#include <stdio.h>
#include <string.h>

#include <string_view>
#include <vector>

using std::string_view;

int main(int argc, char* argv[])
{
    long inputSz = faasmGetInputSize();
    std::vector<uint8_t> inputBuf(inputSz);
    faasmGetInput(inputBuf.data(), inputBuf.size());
    string_view inputStr(reinterpret_cast<char*>(inputBuf.data()), inputBuf.size());
    if (inputStr.size() < 1) {
        const string_view output{"FAILED - no key/value pair. Usage: get_simple with input 'key'"};
        faasmSetOutput(reinterpret_cast<const uint8_t*>(output.data()), output.size());
        return 0;
    }
    const string_view objKey{inputStr};
    uint32_t fetchedLength{};

    uint8_t* objData = __faasmndp_getMmap(reinterpret_cast<const uint8_t*>(objKey.data()), objKey.size(), 1*1024*1024*1024, &fetchedLength);
    if (objData == nullptr) {
        const string_view output{"FAILED - no object found with the given key"};
        faasmSetOutput(reinterpret_cast<const uint8_t*>(output.data()), output.size());
        return 0;
    }

    faasmSetOutput(objData, fetchedLength);
    return 0;
}
