/**
 * @file ex3_relational.cpp
 * @brief Example 3: Relational Operators
 *
 * To run this example, use the `run_experiment` script as detailed in the
 * README.
 *
 * ```
 * ../scripts/run_experiment.sh ... ex3_relational
 * ```
 *
 * This example demonstrates ORQ's relational analytics engine.
 *
 * We assume two secret shared tables (which themselves could be concatenations
 * of multiple data owners' input):
 * 1. A `users` table, which contains sensitive user IDs (such as SSNs), ages,
 * and states;
 * 2. A `payments` table, which contains sensitive user IDs and payment amounts
 *
 * We can imagine that table 1 was contributed in secret-shared form by, e.g.,
 * multiple separate government agencies, while table 2 was contributed by, e.g.
 * credit card companies. Of course, none of the data parties would want to give
 * their data, in the clear, to any of the other parties.
 *
 * The query we want to answer: what is the average amount of money spent, per
 * person between the ages of 18 and 30, in each state?
 *
 * We represent states as arbitrary integers 0 through 50.
 */

#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pID = orq::service::runTime->getPartyID();

    // Sizes of each table.
    size_t n_transactions = 1'000'000;
    size_t population = 100'000;

    // Fewer people so we get more matches.
    const uint MAX_SSN = 999;
    const uint MAX_AMOUNT = 1000;
    const uint NUM_STATES = 51;

    // type alias for template parameters below
    using A = ASharedVector<int>;
    using B = BSharedVector<int>;

    // Storage for the plaintext values
    Vector<int> SSN_p(population), Age(population), State(population);
    Vector<int> SSN_t(n_transactions), Amount(n_transactions);

    // Schemas for the two tables. Names in brackets denote BShared columns
    std::vector<std::string> schema1 = {"[SSN]", "[State]", "[Age]"};
    std::vector<std::string> schema2 = {"[SSN]", "Amount"};

    // For now, P0 generates random data; in reality, these would be input in
    // secret shared form, separately, by data owners
    if (pID == 0) {
        // (This is all plaintext)

        runTime->populateLocalRandom(SSN_t);
        // SSNs have 9 digits
        SSN_t %= MAX_SSN;

        runTime->populateLocalRandom(Amount);
        // Positive amounts only
        Amount %= MAX_AMOUNT;

        // Duplicates? Could ensure uniqueness by contract, or use oblivious
        // distinct
        runTime->populateLocalRandom(SSN_p);
        SSN_p %= MAX_SSN;
        runTime->populateLocalRandom(Age);
        // 'U' here enforces unsigned mod
        Age %= 100U;
        runTime->populateLocalRandom(State);
        State %= NUM_STATES;
    }

    EncodedTable<int> T1 = secret_share<int>({SSN_p, State, Age}, schema1);
    EncodedTable<int> T2 = secret_share<int>({SSN_t, Amount}, schema2);

    // For security, should not open, but if you want to peek at the table,
    // uncomment these lines.
    //
    // print_table(T1.open_with_schema(), pID);
    // print_table(T2.open_with_schema(), pID);

    // Perform the age filter. Watch out for operator precedence with `&`
    T1.filter((T1["[Age]"] >= 18) & (T1["[Age]"] <= 30));

    // We may have duplicate SSNs. Take the youngest.
    T1.aggregate({"[SSN]"}, {{"[Age]", "[Age]", min<B>}});

    // Perform a join on SSN: only select those transactions tied to people of
    // the right age
    // Perform an inner join on key [SSN], and copy [State] over
    auto Joined = T1.inner_join(T2, {"[SSN]"},
                                {
                                    {"[State]", "[State]", copy<B>},
                                });

    // No longer need SSN column
    Joined.deleteColumns({"[SSN]"});
    Joined.addColumn("Population");

    // Aggregate in place.
    Joined.aggregate({"[State]"}, {
                                      {"Amount", "Amount", sum<A>},
                                      {"Population", "Population", count<A>},
                                  });

    // Uncomment to print out intermediate State data
    // print_table(Joined.open_with_schema(), pID);

    // At this point, there is at most one row per state. Sort by valid and trim.
    // This makes division (below) a lot more efficient.
    Joined.sort({ENC_TABLE_VALID}, DESC);
    Joined.head(NUM_STATES);

    Joined.addColumn("[AverageSpend]");

    // Private division is a *boolean* operation, but the C++ operator under the
    // hood calls the a2b conversion protocol
    Joined["[AverageSpend]"] = Joined["Amount"] / Joined["Population"];

    Joined.deleteColumns({"Amount", "Population"});

    // Open & print the result
    // This overload of `print_table` uses the schema to print a nicely-formatted
    // table with headers.
    single_cout("By state, how much do 18-30 year olds spend, on average?");
    print_table(Joined.open_with_schema(), pID);
}