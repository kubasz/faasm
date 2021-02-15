#include "faasm/faasm.h"

#include "ndpapi.h"

#include <stdio.h>
#include <string.h>

#include <string_view>

using std::string_view;

int main(int argc, char* argv[])
{
    const string_view objKey{"testObject1"};
    uint32_t fetchedLength{123};

    uint8_t* objData = __faasmndp_getMmap(reinterpret_cast<const uint8_t*>(objKey.data()), objKey.size(), 1024, &fetchedLength);
    printf("Fetched length = %u\n", fetchedLength);

    if (objData == nullptr) {
        const string_view output{"GetMmap returned null - no object found."};
        faasmSetOutput(reinterpret_cast<const uint8_t*>(output.data()), output.size());
        return 0;
    }

    faasmSetOutput(objData, fetchedLength);

    return 0;
}
