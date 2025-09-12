#pragma once

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

struct RingEntry {
    char buffer[SOCKET_COMMUNICATOR_BUFFER_SIZE];

    size_t used;

    RingEntry() { std::memset(buffer, 0, SOCKET_COMMUNICATOR_BUFFER_SIZE); }
};

class Ring {
    int writeIndex;  // Head
    int readIndex;   // Tail

    int ring_size;
    RingEntry *ring_array;

   public:
    Ring(int);
    ~Ring();

    int getWriteIndex();
    int getReadIndex();

    int nextIndex(int);
    bool isRingFull();
    bool isRingEmpty();

    template <typename T>
    int push(orq::Vector<T>);

    void push(char *, int byte_count);

    RingEntry pop();

    void wait(int);
};

// Ring constructor
Ring::Ring(int r_size) {
    this->writeIndex = 0;
    this->readIndex = 0;

    this->ring_size = r_size;

    this->ring_array = new RingEntry[ring_size];
}

// Ring destructor
Ring::~Ring() { delete[] ring_array; }

int Ring::getWriteIndex() { return writeIndex; }

int Ring::getReadIndex() { return readIndex; }

int Ring::nextIndex(int index) { return (index + 1) % ring_size; }

bool Ring::isRingFull() { return nextIndex(writeIndex) == readIndex; }

bool Ring::isRingEmpty() { return writeIndex == readIndex; }

template <typename T>
int Ring::push(orq::Vector<T> vectorData) {
    static_assert(std::is_same<T, int8_t>::value || std::is_same<T, int16_t>::value ||
                      std::is_same<T, int32_t>::value || std::is_same<T, int64_t>::value ||
                      std::is_same<T, __int128_t>::value,
                  "Ring::push: Invalid type");

    int pushIndex;
    size_t totalBytes = vectorData.size() * sizeof(T);
    size_t bytesProcessed = 0;

    while (bytesProcessed < totalBytes) {
        if (isRingFull()) {
            continue;
        }

        size_t remainingBytes = totalBytes - bytesProcessed;
        size_t bytesToCopy =
            std::min(remainingBytes - (remainingBytes % static_cast<int>(sizeof(T))),
                     SOCKET_COMMUNICATOR_BUFFER_SIZE);

        auto dataPtr = &vectorData[0] + bytesProcessed / sizeof(T);

        std::memcpy(ring_array[writeIndex].buffer, dataPtr, bytesToCopy);
        ring_array[writeIndex].used = bytesToCopy;

        bytesProcessed += bytesToCopy;

        pushIndex = writeIndex;

        writeIndex = nextIndex(writeIndex);
    }

    return pushIndex;
}

void Ring::push(char *buf, int byte_count) {
    while (isRingFull()) {
    }

    std::memcpy(ring_array[writeIndex].buffer, buf, byte_count);
    ring_array[writeIndex].used = byte_count;

    writeIndex = nextIndex(writeIndex);
}

RingEntry Ring::pop() {
    // Block while ring is empty
    while (isRingEmpty()) {
    }

    RingEntry entry = ring_array[readIndex];

    // Increment read index
    readIndex = nextIndex(readIndex);

    return entry;
}

void Ring::wait(int pushIndex) {
    while (readIndex != nextIndex(pushIndex)) {
    }
}
