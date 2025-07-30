#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

struct NoCopyRingEntry {
    const char* buffer;

    size_t buffer_size;
};

class NoCopyRing {
    // NOTE: these should be implemented as lock-free atomics.
    std::atomic<uint32_t> writeIndex = 0;  // Head
    std::atomic<uint32_t> readIndex = 0;   // Tail

    int ring_size;
    NoCopyRingEntry* ring_array;

   public:
    NoCopyRing(int);
    ~NoCopyRing();

    int getWriteIndex() const;
    int getReadIndex() const;

    int nextIndex(int) const;
    bool isRingFull() const;
    bool isRingEmpty() const;

    template <typename T>
    int push(const secrecy::Vector<T>&);

    NoCopyRingEntry* currentEntry() const;

    void pop(NoCopyRingEntry*);

    void wait(int) const;
};

// Ring constructor
NoCopyRing::NoCopyRing(int r_size) {
    this->ring_size = r_size;
    this->ring_array = new NoCopyRingEntry[ring_size];
}

// Ring destructor
NoCopyRing::~NoCopyRing() { delete[] ring_array; }

int NoCopyRing::getWriteIndex() const { return writeIndex; }

int NoCopyRing::getReadIndex() const { return readIndex; }

int NoCopyRing::nextIndex(int index) const { return (index + 1) % ring_size; }

bool NoCopyRing::isRingFull() const { return nextIndex(writeIndex) == readIndex; }

bool NoCopyRing::isRingEmpty() const { return writeIndex == readIndex; }

template <typename T>
int NoCopyRing::push(const secrecy::Vector<T>& vectorData) {
    static_assert(std::is_same<T, int8_t>::value || std::is_same<T, int16_t>::value ||
                      std::is_same<T, int32_t>::value || std::is_same<T, int64_t>::value ||
                      std::is_same<T, __int128_t>::value,
                  "Ring::push: Invalid type");

    int pushIndex;
    size_t totalBytes = vectorData.size() * sizeof(T);

    while (isRingFull()) {
    }

    const auto dataPtr = &vectorData[0];

    ring_array[writeIndex].buffer = reinterpret_cast<const char*>(dataPtr);
    ring_array[writeIndex].buffer_size = totalBytes;

    pushIndex = writeIndex;

    // TODO: Switch to proper atomic increment?
    writeIndex = nextIndex(writeIndex);

    return pushIndex;
}

NoCopyRingEntry* NoCopyRing::currentEntry() const {
    // Block while ring is empty
    while (isRingEmpty()) {
    }

    NoCopyRingEntry* entry = &(ring_array[readIndex]);

    return entry;
}

void NoCopyRing::pop(NoCopyRingEntry* entry) {
    // Block while ring is empty
    while (isRingEmpty()) {
    }

    NoCopyRingEntry* currEntry = &(ring_array[readIndex]);

    if (&(entry->buffer) != &(currEntry->buffer)) {
        throw std::runtime_error(
            "Error: entry->buffer and currEntry->buffer addresses are different");
    }

    ring_array[readIndex].buffer = nullptr;
    ring_array[readIndex].buffer_size = 0;

    // Increment read index
    // TODO: Switch to proper atomic increment?
    readIndex = nextIndex(readIndex);
}

void NoCopyRing::wait(int pushIndex) const {
    // Wait until the read index crosses the index at which the data was pushed
    while (readIndex != nextIndex(pushIndex)) {
    }
}
