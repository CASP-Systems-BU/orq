#include "../../include/secrecy.h"

// VECTOR + BEAVER_2PC
// init_mpc_types(int, gavel::Vector, std::vector<int>, gavel::Tensor, 1)
// init_mpc_system(gavel::SharedMemoryCommunicator, gavel::StoredDataRG, gavel::Beaver_2PC)

// VECTOR + REPLICATED_3PC
//  init_mpc_types(int, gavel::SVec, std::vector<int>, gavel::Tensor, 2)
//  init_mpc_system(gavel::SharedMemoryCommunicator, gavel::StoredDataRG, gavel::Replicated_3PC)

// init_mpc_benchmarking(gavel::benchmarking);

int main() {
    // int batchesNum = 300, batchSize = 100000;

    // // No rounds
    // PrimitivesBenchmarking::benchmark_xor_b(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_add_a(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_sub_a(batchesNum, batchSize);

    // // 1 round
    // PrimitivesBenchmarking::benchmark_and_b(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_or__b(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_mul_a(batchesNum, batchSize);

    // // 6 rounds
    // PrimitivesBenchmarking::benchmark_eq__b(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_neq_b(batchesNum, batchSize);

    // // 7 rounds
    // PrimitivesBenchmarking::benchmark_gt__b(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_lt__b(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_gte_b(batchesNum, batchSize);
    // PrimitivesBenchmarking::benchmark_lte_b(batchesNum, batchSize);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
//  RANDOM Eigen 2pc
//  benchmark_xor_b 		passed in:		2.097807			Throughput:		14300k
//  benchmark_add_a 		passed in:		2.088104			Throughput:		14367k
//  benchmark_sub_a 		passed in:		2.039173			Throughput:		14711k
//  benchmark_and_b 		passed in:		3.156323			Throughput:		9504k
//  benchmark_or__b 		passed in:		3.236834			Throughput:		9268k
//  benchmark_mul_a 		passed in:		3.098019			Throughput:		9683k
//  benchmark_eq__b 		passed in:		10.620580			Throughput:		2824k
//  benchmark_neq_b 		passed in:		10.541090			Throughput:		2846k
//  benchmark_gt__b 		passed in:		11.721307			Throughput:		2559k
//  benchmark_lt__b 		passed in:		11.824420			Throughput:		2537k
//  benchmark_gte_b 		passed in:		11.801994			Throughput:		2541k
//  benchmark_lte_b 		passed in:		11.928569			Throughput:		2514k


//  Random shared ptr 2pc -- not stable
//  benchmark_xor_b 		passed in:		1.417576			Throughput:		21162k
//  benchmark_add_a 		passed in:		1.407634			Throughput:		21312k
//  benchmark_sub_a 		passed in:		1.452923			Throughput:		20648k
//  benchmark_and_b 		passed in:		2.323211			Throughput:		12913k
//  benchmark_or__b 		passed in:		2.211331			Throughput:		13566k
//  benchmark_mul_a 		passed in:		2.268610			Throughput:		13223k

//  Random vector_vector 2pc
//  benchmark_xor_b 	    passed in:		1.437487			Throughput:		20869k
//  benchmark_add_a 	    passed in:		1.397416			Throughput:		21468k
//  benchmark_sub_a 	    passed in:		1.436474			Throughput:		20884k
//  benchmark_and_b 	    passed in:		2.837249			Throughput:		10573k
//  benchmark_or__b 	    passed in:		2.668333			Throughput:		11242k
//  benchmark_mul_a 	    passed in:		2.581394			Throughput:		11621k
//  benchmark_eq__b 	    passed in:		11.904213			Throughput:		2520k
//  benchmark_neq_b 	    passed in:		11.366366			Throughput:		2639k
//  benchmark_gt__b 	    passed in:		12.325826			Throughput:		2433k
//  benchmark_lt__b 	    passed in:		12.610013			Throughput:		2379k
//  benchmark_gte_b 	    passed in:		12.654011			Throughput:		2370k
//  benchmark_lte_b 	    passed in:		12.242702			Throughput:		2450k

//  Random replicated_vector_vector 2pc
//  benchmark_xor_b 		passed in:		1.480410			Throughput:		20264k
//  benchmark_add_a 		passed in:		1.446655			Throughput:		20737k
//  benchmark_sub_a 		passed in:		1.467289			Throughput:		20445k
//  benchmark_and_b 		passed in:		2.757409			Throughput:		10879k
//  benchmark_or__b 		passed in:		2.819974			Throughput:		10638k
//  benchmark_mul_a 		passed in:		2.695893			Throughput:		11128k
//  benchmark_eq__b 		passed in:		11.952025			Throughput:		2510k
//  benchmark_neq_b 		passed in:		11.919943			Throughput:		2516k
//  benchmark_gt__b 		passed in:		13.334320			Throughput:		2249k
//  benchmark_lt__b 		passed in:		13.016071			Throughput:		2304k
//  benchmark_gte_b 		passed in:		13.068559			Throughput:		2295k
//  benchmark_lte_b 		passed in:		13.051230			Throughput:		2298k

//  Random replicated_vector_vector .. benchmark updated 2pc
//  benchmark_xor_b 		passed in:		1.388361			Throughput:		21608k
//  benchmark_add_a 		passed in:		1.306212			Throughput:		22967k
//  benchmark_sub_a 		passed in:		1.376099			Throughput:		21800k
//  benchmark_and_b 		passed in:		2.707738			Throughput:		11079k
//  benchmark_or__b 		passed in:		2.832981			Throughput:		10589k
//  benchmark_mul_a 		passed in:		2.559460			Throughput:		11721k
//  benchmark_eq__b 		passed in:		12.363461			Throughput:		2426k
//  benchmark_neq_b 		passed in:		12.364155			Throughput:		2426k
//  benchmark_gt__b 		passed in:		14.019582			Throughput:		2139k
//  benchmark_lt__b 		passed in:		13.919139			Throughput:		2155k
//  benchmark_gte_b 		passed in:		14.012927			Throughput:		2140k
//  benchmark_lte_b 		passed in:		14.002011			Throughput:		2142k

//  Random replicated_vector_vector 2pc when 3pc worked
//  benchmark_xor_b 		passed in:		1.433646			Throughput:		20925k
//  benchmark_add_a 		passed in:		1.366410			Throughput:		21955k
//  benchmark_sub_a 		passed in:		1.341317			Throughput:		22366k
//  benchmark_and_b 		passed in:		2.662459			Throughput:		11267k
//  benchmark_or__b 		passed in:		2.939327			Throughput:		10206k
//  benchmark_mul_a 		passed in:		2.688566			Throughput:		11158k
//  benchmark_eq__b 		passed in:		14.637795			Throughput:		2049k
//  benchmark_neq_b 		passed in:		14.613785			Throughput:		2052k
//  benchmark_gt__b 		passed in:		14.056026			Throughput:		2134k
//  benchmark_lt__b 		passed in:		13.961810			Throughput:		2148k
//  benchmark_gte_b 		passed in:		14.127260			Throughput:		2123k
//  benchmark_lte_b 		passed in:		13.952633			Throughput:		2150k

//  Random replicated_vector_vector 3pc
//  benchmark_xor_b 		passed in:		2.551866			Throughput:		11756k
//  benchmark_add_a 		passed in:		2.346432			Throughput:		12785k
//  benchmark_sub_a 		passed in:		2.307313			Throughput:		13002k
//  benchmark_and_b 		passed in:		3.266255			Throughput:		9184k
//  benchmark_or__b 		passed in:		3.638452			Throughput:		8245k
//  benchmark_mul_a 		passed in:		3.051477			Throughput:		9831k
//  benchmark_eq__b 		passed in:		13.975990			Throughput:		2146k
//  benchmark_neq_b 		passed in:		14.412574			Throughput:		2081k
//  benchmark_gt__b 		passed in:		15.315596			Throughput:		1958k
//  benchmark_lt__b 		passed in:		15.515078			Throughput:		1933k
//  benchmark_gte_b 		passed in:		15.704591			Throughput:		1910k
//  benchmark_lte_b 		passed in:		15.650020			Throughput:		1916k
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//  No RANDOM Eigen 2pc
//  benchmark_xor_b 		passed in:		1.074643			Throughput:		27916k
//  benchmark_add_a 		passed in:		1.016896			Throughput:		29501k
//  benchmark_sub_a 		passed in:		1.047421			Throughput:		28641k
//  benchmark_and_b 		passed in:		1.721753			Throughput:		17424k
//  benchmark_or__b 		passed in:		1.669234			Throughput:		17972k
//  benchmark_mul_a 		passed in:		1.649529			Throughput:		18187k
//  benchmark_eq__b 		passed in:		6.125417			Throughput:		4897k
//  benchmark_neq_b 		passed in:		5.459214			Throughput:		5495k
//  benchmark_gt__b 		passed in:		5.784308			Throughput:		5186k
//  benchmark_lt__b 		passed in:		5.696142			Throughput:		5266k
//  benchmark_gte_b 		passed in:		5.582463			Throughput:		5373k
//  benchmark_lte_b 		passed in:		5.678026			Throughput:		5283k

//  No Random shared ptr 2pc -- not stable
//  benchmark_xor_b 		passed in:		0.606894			Throughput:		49432k
//  benchmark_add_a 		passed in:		0.593586			Throughput:		50540k
//  benchmark_sub_a 		passed in:		0.678105			Throughput:		44240k
//  benchmark_and_b 		passed in:		0.643142			Throughput:		46645k
//  benchmark_or__b 		passed in:		0.697511			Throughput:		43010k
//  benchmark_mul_a 		passed in:		0.700878			Throughput:		42803k

//  No Random vector_vector 2pc
//  benchmark_xor_b 		passed in:		0.519311			Throughput:		57768k
//  benchmark_add_a 		passed in:		0.527699			Throughput:		56850k
//  benchmark_sub_a 		passed in:		0.505530			Throughput:		59343k
//  benchmark_and_b 		passed in:		0.804173			Throughput:		37305k
//  benchmark_or__b 		passed in:		0.894555			Throughput:		33536k
//  benchmark_mul_a 		passed in:		0.906960			Throughput:		33077k
//  benchmark_eq__b 		passed in:		5.646380			Throughput:		5313k
//  benchmark_neq_b 		passed in:		5.405439			Throughput:		5549k
//  benchmark_gt__b 		passed in:		6.359891			Throughput:		4717k
//  benchmark_lt__b 		passed in:		5.961606			Throughput:		5032k
//  benchmark_gte_b 		passed in:		5.533166			Throughput:		5421k
//  benchmark_lte_b 		passed in:		5.371268			Throughput:		5585k

//  No Random replicated_vector_vector 2pc
//  benchmark_xor_b 		passed in:		0.620097			Throughput:		48379k
//  benchmark_add_a 		passed in:		0.613443			Throughput:		48904k
//  benchmark_sub_a 		passed in:		0.587973			Throughput:		51022k
//  benchmark_and_b 		passed in:		1.111525			Throughput:		26989k
//  benchmark_or__b 		passed in:		1.231315			Throughput:		24364k
//  benchmark_mul_a 		passed in:		0.962606			Throughput:		31165k
//  benchmark_eq__b 		passed in:		5.788166			Throughput:		5182k
//  benchmark_neq_b 		passed in:		5.857630			Throughput:		5121k
//  benchmark_gt__b 		passed in:		6.489353			Throughput:		4622k
//  benchmark_lt__b 		passed in:		6.592237			Throughput:		4550k
//  benchmark_gte_b 		passed in:		6.394465			Throughput:		4691k
//  benchmark_lte_b 		passed in:		6.314713			Throughput:		4750k

//  No Random replicated_vector_vector .. benchmark updated 2pc
//  benchmark_xor_b 		passed in:		0.515489			Throughput:		58197k
//  benchmark_add_a 		passed in:		0.500210			Throughput:		59974k
//  benchmark_sub_a 		passed in:		0.479612			Throughput:		62550k
//  benchmark_and_b 		passed in:		1.097206			Throughput:		27342k
//  benchmark_or__b 		passed in:		1.354564			Throughput:		22147k
//  benchmark_mul_a 		passed in:		1.226383			Throughput:		24462k
//  benchmark_eq__b 		passed in:		7.531306			Throughput:		3983k
//  benchmark_neq_b 		passed in:		7.597603			Throughput:		3948k
//  benchmark_gt__b 		passed in:		8.356833			Throughput:		3589k
//  benchmark_lt__b 		passed in:		8.302815			Throughput:		3613k
//  benchmark_gte_b 		passed in:		10.286770			Throughput:		2916k
//  benchmark_lte_b 		passed in:		9.699706			Throughput:		3092k

//  No Random replicated_vector_vector 2pc when 3pc worked
//  benchmark_xor_b 		passed in:		0.553388			Throughput:		54211k
//  benchmark_add_a 		passed in:		0.538128			Throughput:		55748k
//  benchmark_sub_a 		passed in:		0.503598			Throughput:		59571k
//  benchmark_and_b 		passed in:		1.125925			Throughput:		26644k
//  benchmark_or__b 		passed in:		1.424587			Throughput:		21058k
//  benchmark_mul_a 		passed in:		1.030984			Throughput:		29098k
//  benchmark_eq__b 		passed in:		7.425821			Throughput:		4039k
//  benchmark_neq_b 		passed in:		7.255599			Throughput:		4134k
//  benchmark_gt__b 		passed in:		8.118552			Throughput:		3695k
//  benchmark_lt__b 		passed in:		8.081843			Throughput:		3712k
//  benchmark_gte_b 		passed in:		8.185687			Throughput:		3664k
//  benchmark_lte_b 		passed in:		8.131117			Throughput:		3689k

//  NO Random replicated_vector_vector 3pc
//  benchmark_xor_b 		passed in:		1.400923			Throughput:		21414k
//  benchmark_add_a 		passed in:		1.176390			Throughput:		25501k
//  benchmark_sub_a 		passed in:		1.112091			Throughput:		26976k
//  benchmark_and_b 		passed in:		1.619897			Throughput:		18519k
//  benchmark_or__b 		passed in:		2.135285			Throughput:		14049k
//  benchmark_mul_a 		passed in:		1.621532			Throughput:		18501k
//  benchmark_eq__b 		passed in:		10.760438			Throughput:		2787k
//  benchmark_neq_b 		passed in:		9.750296			Throughput:		3076k
//  benchmark_gt__b 		passed in:		11.022445			Throughput:		2721k
//  benchmark_lt__b 		passed in:		10.893148			Throughput:		2754k
//  benchmark_gte_b 		passed in:		11.051891			Throughput:		2714k
//  benchmark_lte_b 		passed in:		11.224373			Throughput:		2672k
////////////////////////////////////////////////////////////////////////////////////////
