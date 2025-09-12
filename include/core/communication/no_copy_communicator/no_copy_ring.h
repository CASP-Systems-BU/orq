#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

/**
 * @brief A single entry in the NoCopy ring.
 *
 */
struct NoCopyRingEntry {
    const char* buffer;

    size_t buffer_size;
};

/**
 * @brief The NoCopy circular ring buffer. Contains a read and write index, both
 * of which are updated atomically, and a dynamically-allocated array of ring
 * entries.
 *
 * As data is added to the ring buffer, the write index is incremented. As data
 * is read and sent out on the socket, the read index is incremented. If the
 * read index cannot be incremented, the buffer is empty. If the write index
 * cannot be incremented, the buffer is full.
 *
 */
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
    int push(const orq::Vector<T>&);

    NoCopyRingEntry* currentEntry() const;

    void pop(NoCopyRingEntry*);

    void wait(int) const;
};

/**
 * @brief Construct a new NoCopy Ring of the given size.
 *
 * @param r_size
 */
NoCopyRing::NoCopyRing(int r_size) {
    this->ring_size = r_size;
    this->ring_array = new NoCopyRingEntry[ring_size];
}

NoCopyRing::~NoCopyRing() { delete[] ring_array; }

int NoCopyRing::getWriteIndex() const { return writeIndex; }

int NoCopyRing::getReadIndex() const { return readIndex; }

/**
 * @brief Returns the next index in the ring buffer (mod the ring size)
 *
 * @param index
 * @return int
 */
int NoCopyRing::nextIndex(int index) const { return (index + 1) % ring_size; }

/**
 * @brief Checks if the ring is full: that is, `readIndex` is the next index
 * after `writeIndex`.
 *
 * @return true the ring is full
 * @return false it is not
 */
bool NoCopyRing::isRingFull() const { return nextIndex(writeIndex) == readIndex; }

/**
 * @brief Checks if the ring is empty: that is, `readIndex` and `writeIndex` are
 * the same.
 *
 * @return true the ring is empty
 * @return false it is not
 */
bool NoCopyRing::isRingEmpty() const { return writeIndex == readIndex; }

/**
 * @brief Push a (pointer to a) Vector onto the buffer. Be careful about data
 * lifetimes: if the passed vectorData goes out of scope in the caller,
 * undefined data may be sent. This function expects vectorData to remain
 * unmodified until it is sent.
 *
 * @tparam T
 * @param vectorData the const Vector to push onto the buffer.
 * @return int
 */
template <typename T>
int NoCopyRing::push(const orq::Vector<T>& vectorData) {
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

/**
 * @brief Wait until the ring is not empty, then return a pointer to the current
 * entry.
 *
 * @return NoCopyRingEntry*
 */
NoCopyRingEntry* NoCopyRing::currentEntry() const {
    // Block while ring is empty
    while (isRingEmpty()) {
    }

    NoCopyRingEntry* entry = &(ring_array[readIndex]);

    return entry;
}

/**
 * @brief Remove the first element (at readIndex) from the ring buffer.
 *
 * @param entry
 */
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

/**
 * @brief Wait until the readIndex equals pushIndex
 *
 * @param pushIndex
 */
void NoCopyRing::wait(int pushIndex) const {
    // Wait until the read index crosses the index at which the data was pushed
    while (readIndex != nextIndex(pushIndex)) {
    }
}
