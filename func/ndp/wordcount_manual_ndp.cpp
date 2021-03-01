#include "faasm/faasm.h"

#include "ndpapi.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <string_view>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

using std::string_view;

// faasm currently doesn't support passing arguments to functions invoked,
// overcome that with global memory for the time being
struct NdpWorkerComm {
    // input
    string_view objKey;
    // output
    std::unordered_map<std::string, uint32_t> wordCounts;
} workerComm;

int ndp_worker() {
    // input params
    const auto& objKey = workerComm.objKey;
    // output params
    auto& wordCounts = workerComm.wordCounts;
    // worker code [extracted]
    uint32_t fetchedLength{};
    uint8_t* objData = __faasmndp_getMmap(reinterpret_cast<const uint8_t*>(objKey.data()), objKey.size(), 1*1024*1024*1024, &fetchedLength);
    if (objData == nullptr) {
        printf("Error fetching object with key %.*s\n", static_cast<int>(objKey.size()), objKey.data());
        return 1;
    }
    string_view remainingData(reinterpret_cast<char*>(objData), fetchedLength);
    wordCounts = std::unordered_map<std::string, uint32_t>();
    for (;;) {
        while (!remainingData.empty() && remainingData[0] <= ' ') {
            remainingData = remainingData.substr(1);
        }
        if (remainingData.empty()) {break;}
        uint32_t nextSpace{1};
        while (nextSpace < remainingData.size() && remainingData[nextSpace] > ' ') {
            nextSpace++;
        }
        string_view word = remainingData.substr(0, nextSpace);
        remainingData = remainingData.substr(nextSpace);
        wordCounts[std::string(word)] += 1;
    }
    return 0;
}

int worker() {
    long inputSz = faasmGetInputSize();
    std::vector<uint8_t> inputBuf(inputSz);
    faasmGetInput(inputBuf.data(), inputBuf.size());
    const string_view objKey(reinterpret_cast<char*>(inputBuf.data()), inputBuf.size());
    // NDP call into storage
    workerComm = NdpWorkerComm();
    workerComm.objKey = objKey;
    int ndpRetCode = __faasmndp_storageCallAndAwait(&ndp_worker);
    if(ndpRetCode != 0) {
        return ndpRetCode;
    }
    auto& wordCounts = workerComm.wordCounts;
    // sort and output counts
    std::vector<std::pair<std::string, uint32_t>> sortedCounts;
    sortedCounts.reserve(wordCounts.size());
    uint32_t outSz{0};
    for (auto &pair : wordCounts) {
        outSz += pair.first.size() + 5;
        sortedCounts.push_back(std::make_pair(std::string(std::move(pair.first)), pair.second));
    }
    std::sort(sortedCounts.begin(), sortedCounts.end());
    std::vector<char> outputBuf;
    char fmtNumber[16];
    outputBuf.reserve(outSz);
    for (const auto &pair: sortedCounts) {
        outputBuf.insert(outputBuf.end(), pair.first.begin(), pair.first.end());
        outputBuf.push_back('\t');
        int fmtLen = snprintf(fmtNumber, sizeof(fmtNumber), "%u\n", static_cast<unsigned int>(pair.second));
        outputBuf.insert(outputBuf.end(), fmtNumber, fmtNumber + fmtLen);
    }
    outputBuf.push_back('\0');
    outputBuf.push_back('\0');
    faasmSetOutput(reinterpret_cast<const uint8_t*>(outputBuf.data()), outputBuf.size());
    return 0;
}

int main(int argc, char* argv[])
{
    long inputSz = faasmGetInputSize();
    std::vector<uint8_t> inputBuf(inputSz);
    faasmGetInput(inputBuf.data(), inputBuf.size());
    string_view inputStr(reinterpret_cast<char*>(inputBuf.data()), inputBuf.size());
    if (inputStr.size() < 1) {
        const string_view output{"FAILED - no key list. Usage: wordcount key1 key2 key3..."};
        faasmSetOutput(reinterpret_cast<const uint8_t*>(output.data()), output.size());
        return 0;
    }
    std::vector<uint32_t> chainedCalls;
    chainedCalls.reserve(32);
    do {
        size_t nextSpace = inputStr.find_first_of(' ');
        string_view objKey = inputStr.substr(0, nextSpace);
        if (!objKey.empty()) {
            chainedCalls.push_back(faasmChain(&worker, reinterpret_cast<const uint8_t*>(objKey.data()), objKey.size()));
        }
        if (nextSpace >= inputStr.size()) {
            break;
        }
        inputStr = inputStr.substr(nextSpace + 1);
    } while(!inputStr.empty());
    // collect and merge sorted results
    std::map<std::string, uint32_t> sumCounts;
    std::vector<char> resultBuf(4*1024*1024);
    for (uint32_t callId : chainedCalls) {
        int result = faasmAwaitCallOutput(callId, reinterpret_cast<uint8_t*>(resultBuf.data()), resultBuf.size());
        if (result != 0) {
            printf("Error in call %u\n", callId);
            continue;
        }
        int cursor{0};
        while(resultBuf.at(cursor) != '\0') {
            int advance{0};
            int wordLen{0};
            int fCount{0};
            sscanf(resultBuf.data() + cursor, "%*[^\t]%n\t%u\n%n", &wordLen, &fCount, &advance);
            sumCounts[std::string(resultBuf.data() + cursor, wordLen)] += fCount;
            cursor += advance;
        }
    }
    resultBuf.clear();
    resultBuf.reserve(32768);
    char fmtNumber[16];
    for (const auto& pair : sumCounts) {
        resultBuf.insert(resultBuf.end(), pair.first.begin(), pair.first.end());
        resultBuf.push_back('\t');
        int fmtLen = snprintf(fmtNumber, sizeof(fmtNumber), "%u\n", static_cast<unsigned int>(pair.second));
        resultBuf.insert(resultBuf.end(), fmtNumber, fmtNumber + fmtLen);
    }
    faasmSetOutput(reinterpret_cast<const uint8_t*>(resultBuf.data()), resultBuf.size());
    return 0;
}
