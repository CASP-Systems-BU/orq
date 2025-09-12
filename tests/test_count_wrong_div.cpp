#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);

#ifdef MPC_PROTOCOL_BEAVER_TWO
    return 0;
#endif

    // Testing the runtime environment
    const int64_t expSize = 4194;
    const int64_t dataRange = 4194304;
    const int64_t divisor = 1323;

    orq::Vector<int64_t> a_data(expSize, 0);
    for (int i = 0; i < expSize; ++i) {
        a_data[i] = rand() % dataRange;
    }

    ASharedVector<int64_t> a = secret_share_a(a_data, 0);
    ASharedVector<int64_t> res = a / divisor;
    auto res_open = res.open();

    int wrongCount = 0;
    for (int i = 0; i < expSize; ++i) {
        if (res_open[i] != (a_data[i] / divisor)) {
            if (orq::service::runTime->getPartyID() == 0) {
                std::cout << "real: " << a_data[i] / divisor << "\t" << "found: " << res_open[i]
                          << std::endl;
            }
            wrongCount++;
        }
    }

    if (orq::service::runTime->getPartyID() == 0) {
        std::cout << "wrong divisions: " << wrongCount << std::endl;
    }

    return 0;
}
