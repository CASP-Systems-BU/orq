#pragma once

#include <algorithm>
#include <cstring>

#include "constants.h"
#include "coproto/Socket/AsioSocket.h"
#include "correlation_generator.h"
#include "libOTe/Dpf/RegularDpf.h"
#include "libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h"
#include "libOTe/config.h"

// shift up past OPRF ports to avoid overlap
// OPRF uses 4 ports per thread, so we add 4 * MAX_POSSIBLE_THREADS to be safe
const int DPF_BASE_PORT =
    GILBOA_OLE_BASE_PORT + (MAX_POSSIBLE_THREADS * 32) + (MAX_POSSIBLE_THREADS * 4);

namespace orq::random {

/**
 * @brief Distributed Point Function (DPF) generator.
 *
 * @tparam T The type of the input.
 */
template <typename T>
class DPF {
    int rank;

    std::optional<Communicator*> comm;

    // our PRG
    oc::PRNG prng;

    // one socket per role (sender and receiver)
    oc::Socket sock_sender;
    oc::Socket sock_receiver;
    bool isServer;

    // Instances for generating OTs - both parties have both
    std::unique_ptr<OTRecvT> ot_receiver;
    std::unique_ptr<OTSendT> ot_sender;

    // storage for OT results
    std::vector<std::array<oc::block, 2>> base_send_ots;
    std::vector<oc::block> base_recv_ots;
    oc::BitVector base_choices;

    // DPF object and key
    oc::RegularDpf dpf;
    oc::RegularDpfKey key;
    size_t output_bitwidth = 1;

   public:
    /**
     * @brief Constructor for the DPF generator.
     *
     * @param rank The rank of this party.
     * @param thread The thread of this instance.
     * @param _comm The communicator.
     */
    DPF(int rank, int thread, std::optional<Communicator*> _comm = std::nullopt)
        : rank(rank), prng(oc::sysRandomSeed()), comm(_comm) {
        isServer = (rank == 0);

        // setup one socket per role
        int port = DPF_BASE_PORT + 2 * thread;
        auto hostname = std::string((*comm)->host_prefix + ":");
        if (rank == 0) {
            auto addr_sender = hostname + std::to_string(port);
            auto addr_receiver = hostname + std::to_string(port + 1);

            sock_sender = oc::cp::asioConnect(addr_sender, isServer);
            sock_receiver = oc::cp::asioConnect(addr_receiver, isServer);
        } else {
            auto addr_sender = hostname + std::to_string(port + 1);
            auto addr_receiver = hostname + std::to_string(port);

            sock_receiver = oc::cp::asioConnect(addr_receiver, isServer);
            sock_sender = oc::cp::asioConnect(addr_sender, isServer);
        }

        // initialize OT instances - both parties have both roles
        ot_receiver = std::make_unique<OTRecvT>();
        ot_sender = std::make_unique<OTSendT>();
    }

    /**
     * @brief Destructor for the DPF generator.
     *
     * Flushes and closes all sockets.
     */
    ~DPF() {
        // flush and close all sockets
        try {
            oc::cp::sync_wait(sock_sender.flush());
            oc::cp::sync_wait(sock_receiver.flush());
        } catch (const std::exception& e) {
            // ignore exceptions during cleanup
        }

        try {
            oc::cp::sync_wait(sock_sender.close());
            oc::cp::sync_wait(sock_receiver.close());
        } catch (const std::exception& e) {
            // ignore exceptions during cleanup
        }
    }

    /**
     * @brief Reset internal state. Clears stored OT results and DPF key/object.
     *
     * Keeps sockets and OT sender/receiver instances intact.
     */
    void reset() {
        // clear stored OT results
        base_send_ots.clear();
        base_recv_ots.clear();
        base_choices = oc::BitVector();

        // reset DPF object and key
        dpf = oc::RegularDpf();
        key = oc::RegularDpfKey();
        output_bitwidth = 1;
    }

    /**
     * @brief Generate OTs.
     *
     * @param count The number of OTs to generate.
     * @param ot_rank The rank determining the party's role.
     */
    void generateOTs(size_t count, int ot_rank) {
        if (ot_rank == 0) {  // receiver
            base_recv_ots.resize(count);
            base_choices.resize(count);

            // generate random choice bits and receive OT messages
            oc::AlignedVector<oc::block> received_messages(count);

            // randomize choice bits
            base_choices.randomize(prng);

            // receive OT messages based on choice bits
            oc::cp::sync_wait(
                ot_receiver->receive(base_choices, received_messages, prng, sock_receiver));
            oc::cp::sync_wait(sock_receiver.flush());

            // store received messages
            for (size_t i = 0; i < count; i++) {
                base_recv_ots[i] = received_messages[i];
            }
        } else {  // sender
            base_send_ots.resize(count);

            // buffer that OT library will fill in
            oc::AlignedVector<std::array<oc::block, 2>> message_pairs(count);

            // send OT message pairs (libOTe writes into message_pairs)
            oc::cp::sync_wait(ot_sender->send(message_pairs, prng, sock_sender));
            oc::cp::sync_wait(sock_sender.flush());

            // copy out to base_send_ots for setBaseOts
            for (size_t i = 0; i < count; i++) {
                base_send_ots[i] = message_pairs[i];
            }
        }
    }

    /**
     * @brief Getter method for the base OTs in the sender's role.
     *
     * @return The base send OTs.
     */
    oc::span<const std::array<oc::block, 2>> getBaseSendOts() const {
        return oc::span<const std::array<oc::block, 2>>(base_send_ots);
    }

    /**
     * @brief Getter method for the base OTs in the receiver's role.
     *
     * @return The base recv OTs.
     */
    oc::span<const oc::block> getBaseRecvOts() const {
        return oc::span<const oc::block>(base_recv_ots);
    }

    /**
     * @brief Getter method for the base choice in the receiver's role.
     *
     * @return The base choices.
     */
    const oc::BitVector& getBaseChoices() const { return base_choices; }

    /**
     * @brief Non-interactive key generation. One party generates two keys.
     *
     * Assumes that the output is always 1.
     *
     * @param input The input points.
     * @param _domain The domain of the DPF.
     * @return A pair of keys.
     */
    std::array<oc::RegularDpfKey, 2> keyGenNonInteractive(Vector<T> input, uint64_t _domain) {
        oc::u64 domain = _domain;
        oc::u64 numPoints = input.size();

        // convert the inputs to the correct type
        std::vector<T> oc_input = input.as_std_vector();
        // TODO: if T > 64, we need to reinterpret_cast to a vector of oc::u64
        std::vector<oc::u64> points(reinterpret_cast<oc::u64*>(oc_input.data()),
                                    reinterpret_cast<oc::u64*>(oc_input.data() + oc_input.size()));

        // f(a) = 1
        std::vector<oc::block> values(numPoints, oc::toBlock(1));

        // initialize the DPF
        dpf.init(rank, domain, numPoints);

        // create an array of two keys
        std::array<oc::RegularDpfKey, 2> keys;

        // run the non-interactive key generation function
        dpf.keyGen(domain, points, values, prng, keys);

        return keys;
    }

    /**
     * @brief Takes a pair of keys from one party and distributes them to the other party.
     *
     * If maybe_keys is provided, use it as the keys to distribute.
     * The sender should provide two keys.
     * The receiver inputs nothing and receives a key.
     *
     * @param sender_keys The pair of keys to distribute. Flagged as optional,
     *  but required by the sender.
     */
    void distributeKeys(
        const std::optional<std::array<oc::RegularDpfKey, 2>>& sender_keys = std::nullopt) {
        assert(comm.has_value() && "Communicator must be available to distribute keys");

        const bool is_sender = sender_keys.has_value();

        if (is_sender) {
            const auto& keys = sender_keys.value();

            // keep our key based on local rank and send the other
            key = keys[static_cast<size_t>(rank)];
            auto other = keys[static_cast<size_t>(1 - rank)];
            auto payload = serializeKey(other);

            // send domain, numPoints, then size in one header vector, followed by payload
            orq::Vector<int64_t> header(3);
            header[0] = static_cast<int64_t>(dpf.mDomain);
            header[1] = static_cast<int64_t>(dpf.mNumPoints);
            header[2] = static_cast<int64_t>(payload.size());

            comm.value()->sendShares(header, 1);
            comm.value()->sendShares(payload, 1);
        } else {
            // receiver: read domain, numPoints, then size via a single header vector; then payload
            orq::Vector<int64_t> header(3);
            comm.value()->receiveShares(header, 1);
            oc::u64 domain = static_cast<oc::u64>(header[0]);
            oc::u64 numPoints = static_cast<oc::u64>(header[1]);
            oc::u64 words = static_cast<oc::u64>(header[2]);

            dpf.init(rank, domain, numPoints);

            orq::Vector<int64_t> payload(static_cast<size_t>(words));
            comm.value()->receiveShares(payload, 1);
            key = deserializeKey(payload, domain, numPoints);
        }
    }

    /**
     * @brief Interactive key generation. Inputs are secret shared.
     *
     * Converts input types and calls the main DPF DKG function.
     * Default outputs are 1, but they can be overridden.
     *
     * @param input The input secret share.
     * @param _domain The domain of the DPF.
     * @param _values The optional outputs.
     * @param _output_bitwidth The optional output bitwidth.
     */
    void keyGen(Vector<T> input, uint64_t _domain,
                std::optional<std::vector<oc::block>> _values = std::nullopt,
                std::optional<size_t> _output_bitwidth = std::nullopt) {
        // convert the inputs to the correct type
        std::vector<T> oc_input = input.as_std_vector();
        // TODO: if T > 64, we need to reinterpret_cast to a vector of oc::u64
        std::vector<oc::u64> points(reinterpret_cast<oc::u64*>(oc_input.data()),
                                    reinterpret_cast<oc::u64*>(oc_input.data() + oc_input.size()));

        keyGen(points, _domain, _values, _output_bitwidth);
    }

    /**
     * @brief Interactive key generation. Inputs are secret shared.
     *
     * Default outputs are 1, but they can be overridden.
     *
     * @param input The input secret share.
     * @param _domain The domain of the DPF.
     * @param _values The optional outputs.
     * @param _output_bitwidth The optional output bitwidth.
     */
    void keyGen(std::vector<oc::u64> points, uint64_t _domain,
                std::optional<std::vector<oc::block>> _values = std::nullopt,
                std::optional<size_t> _output_bitwidth = std::nullopt) {
        // set the output bitwidth, default to 1
        if (_output_bitwidth.has_value()) {
            output_bitwidth = _output_bitwidth.value();
        }

        oc::u64 domain = _domain;
        oc::u64 numPoints = points.size();

        // if no output values are provided, initialize values as secret sharings of 1
        std::vector<oc::block> values(numPoints, oc::block(0, 0));
        if (!_values.has_value()) {
            // all ones for party 0 and all zeros for party 1
            if (rank == 0) {
                values = std::vector<oc::block>(numPoints, oc::block(0, 1));
            }
        } else {
            values = _values.value();
        }

        // initialize the DPF
        dpf.init(rank, domain, numPoints);

        // generate the OTs
        generateOTs(dpf.baseOtCount(), rank);
        generateOTs(dpf.baseOtCount(), 1 - rank);

        // set the base OTs from our generated OTs
        dpf.setBaseOts(getBaseSendOts(), getBaseRecvOts(), getBaseChoices());

        // interactive key-generation
        auto seed = prng.get<oc::block>();
        if (rank == 0) {
            auto ch = sock_sender.fork();
            oc::cp::sync_wait(dpf.keyGen(points, values, seed, key, ch));
        } else {
            auto ch = sock_receiver.fork();
            oc::cp::sync_wait(dpf.keyGen(points, values, seed, key, ch));
        }
    }

    /**
     * @brief Expand the DPF to the full domain. The output is a unit vector.
     *
     * @return A vector of unit vectors of long bitwidth. Unit vectors are stored as block vectors.
     */
    std::vector<std::vector<oc::block>> expand() {
        oc::u64 domain = dpf.mDomain;
        oc::u64 numPoints = dpf.mNumPoints;

        // calculate the number of 128-bit blocks per row (pack output_bitwidth bits per index)
        size_t blocksPerRow = static_cast<size_t>(((domain * output_bitwidth) + 127) / 128);

        // initialize the output
        std::vector<std::vector<oc::block>> sparse_results(
            static_cast<size_t>(numPoints), std::vector<oc::block>(blocksPerRow, oc::block(0, 0)));

        // expand the DPF
        oc::RegularDpf::expand(
            oc::u64(rank), domain, key, [&](auto k, auto i, oc::block v, oc::block t) {
                // calculate the row and bit offset for the current value
                size_t row = static_cast<size_t>(k);
                size_t idx = static_cast<size_t>(i);

                // each logical index contributes output_bitwidth bits
                size_t bitStart = idx * output_bitwidth;
                size_t blockIndex = bitStart / 128;
                size_t bitInBlock = bitStart % 128;

                // ensure we never spill into the next 128-bit block
                assert(bitInBlock + output_bitwidth <= 128);
                assert(output_bitwidth <= 64);

                // extract least-significant output_bitwidth bits from v into 64-bit halves
                uint64_t v_lo = v.get<oc::u64>(0);
                uint64_t v_hi = v.get<oc::u64>(1);

                uint64_t val_lo = 0;
                uint64_t val_hi = 0;
                val_lo =
                    (output_bitwidth == 64) ? ~uint64_t(0) : ((uint64_t(1) << output_bitwidth) - 1);
                val_lo &= v_lo;

                // place into the packed result starting at bitInBlock of blockIndex using 64-bit
                // shifts
                if (bitInBlock < 64) {
                    uint64_t lo_part = val_lo << bitInBlock;
                    uint64_t hi_part = (bitInBlock == 0) ? 0 : (val_lo >> (64 - bitInBlock));
                    sparse_results[row][blockIndex] ^= oc::block(hi_part, lo_part);
                } else {
                    size_t off = bitInBlock - 64;
                    uint64_t hi_part = val_lo << off;
                    sparse_results[row][blockIndex] ^= oc::block(hi_part, 0);
                }
            });

        return sparse_results;
    }

    /**
     * @brief Expand the DPF to the full domain. The output is a matrix of values and tags.
     *
     * @return A pair of matrices, one for values and one for tags.
     */
    std::pair<oc::Matrix<oc::block>, oc::Matrix<oc::u8>> expandFullMatrix() {
        oc::u64 domain = dpf.mDomain;
        oc::u64 numPoints = dpf.mNumPoints;

        // initialize the output matrices
        oc::Matrix<oc::block> values;
        oc::Matrix<oc::u8> tags;
        values.resize(numPoints, domain);
        tags.resize(numPoints, domain);

        // expand the DPF
        oc::RegularDpf::expand(oc::u64(rank), domain, key,
                               [&](auto k, auto i, oc::block v, oc::block t) {
                                   // just copy values into the output matrices
                                   values(k, i) = v;
                                   tags(k, i) = t.get<oc::u8>(0) & 1;
                               });

        return std::make_pair(values, tags);
    }

    /**
     * Correctness check for the sparse/compressed view of the full domain expansion.
     *
     * When we evaluate over the full domain, we XOR all values in a row in the matrix.
     *
     * @param result A vector of unit vectors of long bitwidth.
     */
    void assertCorrelated(std::vector<std::vector<oc::block>>& result) {
        if (!comm.has_value()) {
            return;
        }

        oc::u64 domain = dpf.mDomain;
        oc::u64 numPoints = dpf.mNumPoints;

        // calculate the number of 128-bit blocks per row
        size_t blocksPerRow = static_cast<size_t>((domain + 127) / 128);

        for (size_t r = 0; r < static_cast<size_t>(numPoints); ++r) {
            orq::Vector<__int128_t> row(blocksPerRow);
            for (size_t b = 0; b < blocksPerRow; ++b) {
                row[b] = result[r][b].get<__int128_t>(0);
            }

            // exchange once per element in the input
            // inefficient, but this is a test
            orq::Vector<__int128_t> other(blocksPerRow);
            comm.value()->exchangeShares(row, other, 1, blocksPerRow);

            // compute the plaintext
            orq::Vector<__int128_t> plaintext = row ^ other;

            int hamming_weight = 0;
            for (size_t b = 0; b < blocksPerRow; ++b) {
                __int128_t u = static_cast<__int128_t>(plaintext[b]);
                for (int bit = 0; bit < 128; ++bit) {
                    hamming_weight += static_cast<int>((u >> bit) & 1);
                }
            }
            assert(hamming_weight == 1);
        }
    }

    /**
     * Correctness check for the matrix view of the full domain expansion.
     *
     * When we evaluate over the full domain, we get a matrix of values and tags.
     * Dimensions are (numPoints, domain).
     *
     * @param result A pair of matrices from the full domain expansion.
     */
    void assertCorrelated(std::pair<oc::Matrix<oc::block>, oc::Matrix<oc::u8>>& result) {
        if (!comm.has_value()) {
            return;
        }

        auto [values, tags] = result;

        oc::u64 domain = dpf.mDomain;
        oc::u64 numPoints = dpf.mNumPoints;

        int rows = tags.size() / tags[0].size();

        // send the tags to the other party
        orq::Vector<int8_t> tags_vec(tags.size());
        for (int i = 0; i < tags.size(); i++) {
            tags_vec[i] = (int8_t)tags(i);
        }
        orq::Vector<int8_t> other_tags_vec(tags.size());
        comm.value()->exchangeShares(tags_vec, other_tags_vec, 1);

        // compute the plaintext tags
        orq::Vector<int8_t> plaintext_tags = tags_vec ^ other_tags_vec;

        // check that the tags are correct
        int index = 0;
        for (int i = 0; i < rows; i++) {
            int sum = 0;
            for (int j = 0; j < tags[0].size(); j++) {
                sum += plaintext_tags[index];
                index++;
            }
            assert(sum == 1);
        }

        // send the values to the other party
        orq::Vector<__int128_t> values_vec(values.size());
        for (int i = 0; i < values.size(); i++) {
            values_vec[i] = values(i).get<__int128_t>(0);
        }
        orq::Vector<__int128_t> other_values_vec(values.size());
        comm.value()->exchangeShares(values_vec, other_values_vec, 1);

        // compute the plaintext values
        orq::Vector<__int128_t> plaintext_values = values_vec ^ other_values_vec;

        // check that the values are correct
        index = 0;
        for (int i = 0; i < rows; i++) {
            int sum = 0;
            for (int j = 0; j < values[0].size(); j++) {
                sum += plaintext_values[index];
                index++;
            }
            assert(sum == 1);
        }
    }

   private:
    /**
     * Serialize a DPF key to a vector of int64_t.
     * Uses libOTe's RegularDpfKey::toBytes.
     *
     * The key is serialized as follows:
     * - byte_size: the number of bytes in the key
     * - payload: the key data
     *
     * @param k The key to serialize.
     * @return The serialized key.
     */
    static orq::Vector<int64_t> serializeKey(oc::RegularDpfKey& k) {
        const size_t byte_size = k.sizeBytes();
        const size_t words = (byte_size + sizeof(int64_t) - 1) / sizeof(int64_t);

        // layout: [byte_size][payload words...]
        orq::Vector<int64_t> out(words + 1);
        out[0] = static_cast<int64_t>(byte_size);

        std::vector<uint8_t> buf(byte_size);
        oc::span<oc::u8> span_out(reinterpret_cast<oc::u8*>(buf.data()), buf.size());
        k.toBytes(span_out);

        for (size_t i = 0; i < words; ++i) {
            int64_t w = 0;
            size_t offset = i * sizeof(int64_t);
            size_t remain = (offset < byte_size) ? (byte_size - offset) : 0;
            size_t to_copy = remain >= sizeof(int64_t) ? sizeof(int64_t) : remain;
            if (to_copy) {
                std::memcpy(&w, buf.data() + offset, to_copy);
            }
            out[i + 1] = w;
        }
        return out;
    }

    /**
     * Deserialize a DPF key from a vector of int64_t.
     * Uses libOTe's RegularDpfKey::fromBytes.
     *
     * The key is deserialized as follows:
     * - byte_size: the number of bytes in the key
     * - payload: the key data
     *
     * @param v The vector of int64_t to deserialize.
     * @param domain The domain of the DPF.
     * @param numPoints The number of points of the DPF.
     * @return The deserialized key.
     */
    static oc::RegularDpfKey deserializeKey(const orq::Vector<int64_t>& v, oc::u64 domain,
                                            oc::u64 numPoints) {
        assert(v.size() >= 1);
        const size_t byte_size = static_cast<size_t>(v[0]);
        const size_t words = v.size() - 1;

        std::vector<uint8_t> buf(byte_size);
        for (size_t i = 0; i < words; ++i) {
            int64_t w = v[i + 1];
            size_t offset = i * sizeof(int64_t);
            size_t remain = (offset < byte_size) ? (byte_size - offset) : 0;
            size_t to_copy = remain >= sizeof(int64_t) ? sizeof(int64_t) : remain;
            if (to_copy) {
                std::memcpy(buf.data() + offset, &w, to_copy);
            }
        }

        oc::RegularDpfKey k;
        k.resize(domain, numPoints);
        k.fromBytes(oc::span<oc::u8>(reinterpret_cast<oc::u8*>(buf.data()), buf.size()));
        return k;
    }
};
}  // namespace orq::random
