#ifndef SECRECY_PRG_ALGORITHM_H
#define SECRECY_PRG_ALGORITHM_H

#include <math.h>
#include <sodium.h>
#include <stdlib.h>

#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <span>

#define __DEFAULT_PRGALGORITHM_BUFFER_SIZE (1 << 20)
#define __MAX_AES_QUERY_BYTES (1 << 20)
#define __MAX_DEV_URANDOM_QUERY_BYTES (1 << 20)
// TODO This max query size has not been thoroughly tested.
#define __MAX_XCHACHA20_QUERY_BYTES (1 << 20)

namespace secrecy::random {

class PRGAlgorithm {
public:
    /**
     * fillBytes - Fills dest with random data.
     */
    virtual void fillBytes(std::span<uint8_t> dest) = 0;

    /**
     * getNext - Generates random bytes to fill one T.
     * 
     * @param num The reference to fill with random bytes.
     */
    template <typename T>
    void getNext(T& num) {
        fillBytes(std::span<uint8_t>(reinterpret_cast<uint8_t *>(&num), sizeof(T)));
    }

    /**
     * getNext - Fills nums with random data.
     * 
     * This function creates a buffer to hold the random values before copying
     * them to the Vector.
     */
    template <typename T>
    void getNext(Vector<T>& nums) {
        size_t preferred_size = getPreferredBufferSize();
        if (thread_buffer.size() < preferred_size) {
            thread_buffer.resize(preferred_size);
        }
        getNextBuffered(nums, std::span<uint8_t>(thread_buffer.data(), thread_buffer.size()));
    }

protected:
    static inline thread_local std::vector<uint8_t> thread_buffer;

    /**
     * getPreferredBufferSize - Returns the size of the buffer allocated to
     * temporarily store random numbers before they are copied to Vectors.
     */
    virtual size_t getPreferredBufferSize() {
        return __DEFAULT_PRGALGORITHM_BUFFER_SIZE;
    }

private:
    /**
     * getNextBuffered - Fills nums with random data.
     * 
     * @param nums The Vector to fill.
     * @param buffer A buffer to hold the randomly generated values before they
     * are copied into the Vector. It must be large enough to contain at least
     * one element (`sizeof(T)`). This buffer is necessary because the Vector
     * may be a non-contiguous view of a block of memory, and therefore that
     * memory can't be filled directly with the random data.
     */
    template <typename T>
    void getNextBuffered(Vector<T>& nums, std::span<uint8_t> buffer) {
        size_t element_size = sizeof(T);
        assert(!buffer.empty());
        assert(buffer.size() >= element_size);
        size_t index = 0;
        size_t total_elements = nums.size();
        size_t max_elements_in_buffer = buffer.size() / element_size;
        while (index < total_elements) {
            size_t elements_remaining = total_elements - index;
            size_t elements_this_batch = std::min(max_elements_in_buffer, elements_remaining);
            size_t batch_bytes = elements_this_batch * element_size;
            fillBytes(buffer.subspan(0, batch_bytes));
            for (size_t i = 0; i < elements_this_batch; i++) {
                std::memcpy(&nums[index + i], &buffer[i * element_size], element_size);
            }
            index += elements_this_batch;
        }
    }
};

class DeterministicPRGAlgorithm: public PRGAlgorithm {
public:
    /**
     * setSeed - Sets the seed for generation of random numbers.
     */
    virtual void setSeed(std::vector<unsigned char>& seed) = 0;

    virtual ~DeterministicPRGAlgorithm() = default;

    /**
     * incrementNonce - Increment the algorithm's nonce, if applicable.
     * 
     * If this algorithm has no nonce, this is a nop. Used to keep PRGs in sync
     * when shared by multiple parties.
     */
    virtual void incrementNonce() = 0;
};

class AESPRGAlgorithm: public DeterministicPRGAlgorithm {
public:
    /*
        This maximum size has been determined empirically. If we attempt to
        fill a vector much larger than this, our call to the libsodium AES
        function will segfault. This maximum is set several orders of magnitude
        lower than the size at which errors occured in testing to account for
        potential differences in hardware.
    */
    static inline const size_t MAX_AES_QUERY_BYTES = __MAX_AES_QUERY_BYTES;

    /**
     * AESPRGAlgorithm - Creates an AESPRGAlgorithm object.
     * 
     * @param _seed The seed shared between the parties.
     */
    AESPRGAlgorithm(std::vector<unsigned char>& _seed) : nonce(0) {
        setSeed(_seed);
    }

    void fillBytes(std::span<uint8_t> dest) override {
        size_t bytes = dest.size();
        size_t batches = (bytes / MAX_AES_QUERY_BYTES) + (bytes % MAX_AES_QUERY_BYTES != 0);
        size_t bytes_remaining = bytes;
        size_t index = 0;
        for (size_t batch = 0; batch < batches; batch++) {
            size_t batch_bytes = std::min(bytes_remaining, MAX_AES_QUERY_BYTES);
            aesGenerateValues(dest.subspan(index, batch_bytes));
            bytes_remaining -= batch_bytes;
            index += batch_bytes;
        }
    }

    void setSeed(std::vector<unsigned char>& _seed) override {
        assert(_seed.size() <= crypto_aead_aes256gcm_KEYBYTES);
        std::copy(_seed.begin(), _seed.end(), seed);
    }

    void incrementNonce() override {
        nonce++;
    }

    static void aesKeyGen(std::span<unsigned char> key) {
        if (key.size() != crypto_aead_aes256gcm_KEYBYTES) {
            throw std::runtime_error("Key buffer has incorrect size");
        }
        crypto_aead_aes256gcm_keygen(key.data());
    }

protected:
    size_t getPreferredBufferSize() override {
        return __MAX_AES_QUERY_BYTES;
    }

private:
    // the seed shared between the parties
    unsigned char seed[crypto_aead_aes256gcm_KEYBYTES] = {};

    // nonce, essentially a sequence number
    // initialized to zero and incremented with each call to the CommonPRG
    unsigned long nonce;

    void aesGenerateValues(std::span<uint8_t> dest) {
        // get the nonce as a char array
        unsigned char nonce_char[crypto_aead_aes256gcm_NPUBBYTES];
        unsigned long nonce_temp = nonce;
        for (int byte = 0; byte < crypto_aead_aes256gcm_NPUBBYTES; byte++) {
            nonce_char[byte] = nonce_temp & 0xFF;
            nonce_temp >>= 8;
        }
        unsigned long long message_len = dest.size();
        // MAC is ignored and not used
        unsigned char mac[crypto_aead_aes256gcm_ABYTES];
        // This will be set to zero on init and never modified.
        static const std::array<uint8_t, MAX_AES_QUERY_BYTES> zero_message = {0};
        // call AES
        crypto_aead_aes256gcm_encrypt_detached(
            dest.data(), mac, NULL, zero_message.data(), message_len, NULL, 0, NULL,
            nonce_char, seed);
        nonce++;
    }
};

class DevUrandomPRGAlgorithm : public PRGAlgorithm {
    /*
        This maximum size has been determined empirically. If we attempt to
        fill a vector much larger than this, our call to /dev/urandom
        will segfault. This maximum is set several orders of magnitude
        lower than the size at which errors occurred in testing to account for
        potential differences in hardware.
    */
    static inline const size_t MAX_DEV_URANDOM_QUERY_BYTES = __MAX_DEV_URANDOM_QUERY_BYTES;

public:
    void fillBytes(std::span<uint8_t> dest) override {
        size_t bytes = dest.size();
        size_t batches = (bytes / MAX_DEV_URANDOM_QUERY_BYTES) + (bytes % MAX_DEV_URANDOM_QUERY_BYTES != 0);
        size_t bytes_remaining = bytes;
        size_t index = 0;
        std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
        if (!urandom) {
            std::cerr << "Failed to open /dev/urandom" << std::endl;
            abort();
        }
        for (size_t batch = 0; batch < batches; batch++) {
            size_t batch_bytes = std::min(bytes_remaining, MAX_DEV_URANDOM_QUERY_BYTES);
            urandom.read(reinterpret_cast<char *>(dest.subspan(index, batch_bytes).data()), batch_bytes);
            bytes_remaining -= batch_bytes;
            index += batch_bytes;
        }
    }
protected:
    size_t getPreferredBufferSize() override {
        return __MAX_DEV_URANDOM_QUERY_BYTES;
    }
};

class XChaCha20PRGAlgorithm : public DeterministicPRGAlgorithm {
public:
    static inline const size_t MAX_XCHACHA20_QUERY_BYTES = __MAX_XCHACHA20_QUERY_BYTES;

    /**
     * XChaCha20PRGAlgorithm - Creates an XChaCha20PRGAlgorithm object.
     * 
     * @param _seed The seed shared between the parties.
     */
    XChaCha20PRGAlgorithm(std::vector<unsigned char>& _seed) : nonce(0) {
        setSeed(_seed);
    }

    void fillBytes(std::span<uint8_t> dest) override {
        size_t bytes = dest.size();
        size_t batches = (bytes / MAX_XCHACHA20_QUERY_BYTES) + (bytes % MAX_XCHACHA20_QUERY_BYTES != 0);
        size_t bytes_remaining = bytes;
        size_t index = 0;
        for (size_t batch = 0; batch < batches; batch++) {
            size_t batch_bytes = std::min(bytes_remaining, MAX_XCHACHA20_QUERY_BYTES);
            xchacha20GenerateValues(dest.subspan(index, batch_bytes));
            bytes_remaining -= batch_bytes;
            index += batch_bytes;
        }
    }

    void setSeed(std::vector<unsigned char>& _seed) override {
        assert(_seed.size() <= crypto_stream_xchacha20_KEYBYTES);
        std::copy(_seed.begin(), _seed.end(), seed);
    }

    void incrementNonce() override {
        nonce++;
    }

    static void xchacha20KeyGen(std::span<unsigned char> key) {
        if (key.size() != crypto_stream_xchacha20_KEYBYTES) {
            throw std::runtime_error("Key buffer has incorrect size");
        }
        crypto_stream_xchacha20_keygen(key.data());
    }

protected:
    size_t getPreferredBufferSize() override {
        return __MAX_XCHACHA20_QUERY_BYTES;
    }

private:
    // the seed shared between the parties
    unsigned char seed[crypto_stream_xchacha20_KEYBYTES] = {};

    // nonce, essentially a sequence number
    // initialized to zero and incremented with each call to the CommonPRG
    unsigned long nonce;

    void xchacha20GenerateValues(std::span<uint8_t> dest) {
        // get the nonce as a char array
        unsigned char nonce_char[crypto_stream_xchacha20_NONCEBYTES];
        unsigned long nonce_temp = nonce;
        for (int byte = 0; byte < crypto_stream_xchacha20_NONCEBYTES; byte++) {
            nonce_char[byte] = nonce_temp & 0xFF;
            nonce_temp >>= 8;
        }
        unsigned long long message_len = dest.size();
        crypto_stream_xchacha20(dest.data(), message_len, nonce_char, seed);
        nonce++;
    }
};
}

#endif
