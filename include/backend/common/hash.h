#pragma once

#include <sodium.h>

#include <array>
#include <span>

#include "core/communication/communicator.h"

namespace orq {

/**
 * @brief Wrapper around crypto_generichash
 *
 */
class Hash {
   private:
    crypto_generichash_state state;
    bool finalized = false;

   public:
    /**
     * @brief Keyless constructor
     *
     */
    Hash() {
        crypto_generichash_init(&state, NULL, 0, crypto_generichash_BYTES);
        // TODO: domain separation
    }

    /**
     * @brief Keyed constructor
     *
     */
    Hash(std::array<uint8_t, crypto_generichash_KEYBYTES>& k) {
        crypto_generichash_init(&state, k.data(), k.size(), crypto_generichash_BYTES);
    }

    /**
     * @brief Static method which generates a hash into an std::array
     *
     * @tparam T A span type of hashable elements
     * @param x
     * @return std::array<uint8_t, crypto_generichash_BYTES>
     */
    template <typename T>
    static std::array<uint8_t, crypto_generichash_BYTES> generateHashArray(T&& x) {
        auto h = Hash();
        h.update(x);
        return h.finalizeArray();
    }

    /**
     * @brief Static method which generates a hash into an orq Vector
     *
     * @tparam T A span type of hashable elements
     * @param x
     * @return orq::Vector<uint8_t>
     */
    template <typename T>
    static orq::Vector<uint8_t> generateHash(T&& x) {
        auto h = Hash();
        h.update(x);
        return h.finalize();
    }

    /**
     * @brief Update with an arbitrary contiguous span<T>
     *
     * @tparam T
     * @tparam Length of the span
     * @param s
     * @return requires span<T> is trivially copyable (contiguous memory layout)
     */
    template <typename T, size_t Extent>
        requires std::is_trivially_copyable_v<T>
    void update(std::span<T, Extent> s) {
        if (finalized) {
            throw std::logic_error("Cannot update finalized hash!");
        }

        const auto bytes = std::as_bytes(s);
        crypto_generichash_update(&state, reinterpret_cast<const unsigned char*>(bytes.data()),
                                  bytes.size());
    }

    /**
     * @brief Special overload for GF2E (4PC verifier protocol). Serialize & pass span to standard
     * method.
     *
     * @param s
     */
    void update(std::span<const NTL::GF2E> s) { update(serializeGF2E(s).span()); }

    /**
     * @brief Finalize this hash and get a byte-vector output. Hash object cannot be used again.
     *
     * @return orq::Vector<uint8_t>
     */
    orq::Vector<uint8_t> finalize() {
        finalized = true;

        orq::Vector<uint8_t> v(crypto_generichash_BYTES);
        crypto_generichash_final(&state, v.data(), v.size());

        return v;
    }

    /**
     * @brief Array version of the above.
     *
     * @return std::array<uint8_t, crypto_generichash_BYTES>
     */
    std::array<uint8_t, crypto_generichash_BYTES> finalizeArray() {
        finalized = true;

        std::array<uint8_t, crypto_generichash_BYTES> out{};
        crypto_generichash_final(&state, out.data(), out.size());

        return out;
    }
};
}  // namespace orq