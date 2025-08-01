#include "../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

const int test_size = 1 << 16;

template <typename T>
void TestBasicCommunication(const int& testSize) {
    auto n = secrecy::service::runTime->getNumParties();
    auto othersCount = n - 1;

    // Exchanging one vector
    {
        secrecy::Vector<T> x(testSize), y(testSize), z(testSize);
        runTime->populateLocalRandom(x);

        // Exchange Shares with party +1
        runTime->comm0()->exchangeShares(x, y, +1, -1, testSize);

        // Exchange Shares with party -1
        runTime->comm0()->exchangeShares(y, z, -1, +1, testSize);

        // I should get the vector that I have just sent
        if (runTime->getPartyID() == 0) {
            assert(x.same_as(z));
        }
    }

    // Exhange multiple vectors
    {
        std::vector<secrecy::Vector<T>> x, y, z;
        for (int i = 0; i < othersCount; i++) {
            x.push_back(secrecy::Vector<T>(testSize));
            y.push_back(secrecy::Vector<T>(testSize));
            z.push_back(secrecy::Vector<T>(testSize));
            runTime->populateLocalRandom(x[i]);
        }

        std::vector<secrecy::PartyID> to(othersCount);
        std::vector<secrecy::PartyID> from(othersCount);
        for (int i = 0; i < othersCount; i++) {
            to[i] = i + 1;
            from[i] = -i - 1;
        }

        // Exchange Shares with parties +1, +2, +3
        runTime->comm0()->exchangeShares(x, y, to, from);

        // Exchange Shares with parties -1, -2, -3
        runTime->comm0()->exchangeShares(y, z, from, to);

        // I should get the vectors that I have just sent
        if (runTime->getPartyID() == 0) {
            for (int i = 0; i < othersCount; i++) {
                assert(x[i].same_as(z[i]));
            }
        }
    }
}

int main(int argc, char** argv) {
    // Initialize Secrecy runtime
    secrecy_init(argc, argv);

    TestBasicCommunication<int8_t>(test_size);
    single_cout("int8_t communication: OK");

    TestBasicCommunication<int16_t>(test_size);
    single_cout("int16_t communication: OK");

    TestBasicCommunication<int32_t>(test_size);
    single_cout("int32_t communication: OK");

    TestBasicCommunication<int64_t>(test_size);
    single_cout("int64_t communication: OK");

    TestBasicCommunication<__int128_t>(test_size);
    single_cout("__int128_t communication: OK");

// Tear down communication
#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}
