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
    size_t keyEnd = inputStr.find_first_of(' ');
    if (inputStr.size() < 2 || keyEnd == string_view::npos) {
        const string_view output{"FAILED - no key/value pair. Usage: put_simple with input 'key value'"};
        faasmSetOutput(reinterpret_cast<const uint8_t*>(output.data()), output.size());
        return 2;
    }
    const string_view objKey{inputStr.substr(0, keyEnd)};
    const string_view objValue{inputStr.substr(keyEnd + 1)};

    int32_t result = __faasmndp_put(reinterpret_cast<const uint8_t*>(objKey.data()), objKey.size(), reinterpret_cast<const uint8_t*>(objValue.data()), objValue.size());

    if (result != 0) {
        const string_view output{"Error creating/updating the object with the given key"};
        faasmSetOutput(reinterpret_cast<const uint8_t*>(output.data()), output.size());
        return 1;
    }

    const string_view output{"OK"};
    faasmSetOutput(reinterpret_cast<const uint8_t*>(output.data()), output.size());

    return 0;
}
