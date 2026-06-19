// Scratchpad file
// Git ignored, so you can use this for quick experiments, e.g.
//   ../scripts/run_experiment.py -p 3 scratch

#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    Vector<int> x(10);
    ASharedVector<int> y = secret_share_a(x, 0);
    BSharedVector<int> z = secret_share_b(x, 0);

    y *= y;

    single_cout("rounds = " << runTime->comm0()->getRounds());
    print(y.open(), pID);
    single_cout("rounds = " << runTime->comm0()->getRounds());

    single_cout("rounds = " << runTime->comm0()->getRounds());

    return 0;
}
