#include "../../include/secrecy.h"

using namespace secrecy::service::mpi_service::replicated_3pc;init_mpc_benchmarking(secrecy::service::benchmarking);


int main(int argc, char **argv) {
    secrecy_init(argc, argv);

    int batchesNum = 50, batchSize = 1000000;

    // No rounds
    PrimitivesBenchmarking::benchmark_xor_b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_add_a(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_sub_a(batchesNum, batchSize);

    // 1 round
    PrimitivesBenchmarking::benchmark_and_b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_or__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_mul_a(batchesNum, batchSize);

    // 6 rounds
    PrimitivesBenchmarking::benchmark_eq__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_neq_b(batchesNum, batchSize);

    // 7 rounds
    PrimitivesBenchmarking::benchmark_gt__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_lt__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_gte_b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_lte_b(batchesNum, batchSize);

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif

    return 0;
}


// NO Random 3PC    500, 100k
//benchmark_xor_b 		passed in:		1.290543			Throughput:		38743k
//benchmark_add_a 		passed in:		1.118992			Throughput:		44683k
//benchmark_sub_a 		passed in:		1.168610			Throughput:		42785k
//benchmark_and_b 		passed in:		1.365904			Throughput:		36605k
//benchmark_or__b 		passed in:		1.914293			Throughput:		26119k
//benchmark_mul_a 		passed in:		1.290518			Throughput:		38744k
//benchmark_eq__b 		passed in:		8.387213			Throughput:		5961k
//benchmark_neq_b 		passed in:		8.794710			Throughput:		5685k
//benchmark_gt__b 		passed in:		9.960493			Throughput:		5019k
//benchmark_lt__b 		passed in:		9.394675			Throughput:		5322k
//benchmark_gte_b 		passed in:		8.738211			Throughput:		5721k
//benchmark_lte_b 		passed in:		8.902406			Throughput:		5616k


// NO Random 3PC    500, 100k   MPI_parallelism_factor = 1
//benchmark_xor_b 		passed in:		1.290786			Throughput:		38736k
//benchmark_add_a 		passed in:		1.094510			Throughput:		45682k
//benchmark_sub_a 		passed in:		1.191010			Throughput:		41981k
//benchmark_and_b 		passed in:		1.350348			Throughput:		37027k
//benchmark_or__b 		passed in:		1.919239			Throughput:		26051k
//benchmark_mul_a 		passed in:		1.299339			Throughput:		38481k
//benchmark_eq__b 		passed in:		8.859079			Throughput:		5643k
//benchmark_neq_b 		passed in:		8.532991			Throughput:		5859k
//benchmark_gt__b 		passed in:		8.553179			Throughput:		5845k
//benchmark_lt__b 		passed in:		8.525605			Throughput:		5864k
//benchmark_gte_b 		passed in:		8.918493			Throughput:		5606k
//benchmark_lte_b 		passed in:		9.578440			Throughput:		5220k

// NO Random 3PC    500, 100k   MPI_parallelism_factor = 1
//benchmark_xor_b 		passed in:		1.252788			Throughput:		39910k
//benchmark_add_a 		passed in:		1.098869			Throughput:		45501k
//benchmark_sub_a 		passed in:		1.204905			Throughput:		41497k
//benchmark_and_b 		passed in:		1.587944			Throughput:		31487k
//benchmark_or__b 		passed in:		1.929149			Throughput:		25918k
//benchmark_mul_a 		passed in:		1.275982			Throughput:		39185k
//benchmark_eq__b 		passed in:		8.369751			Throughput:		5973k
//benchmark_neq_b 		passed in:		8.087239			Throughput:		6182k
//benchmark_gt__b 		passed in:		8.516597			Throughput:		5870k
//benchmark_lt__b 		passed in:		8.539275			Throughput:		5855k
//benchmark_gte_b 		passed in:		8.644264			Throughput:		5784k
//benchmark_lte_b 		passed in:		8.618529			Throughput:		5801k


// NO Random 3PC    500, 100k   MPI_parallelism_factor = 2
//benchmark_xor_b 		passed in:		1.333629			Throughput:		37491k
//benchmark_add_a 		passed in:		1.129857			Throughput:		44253k
//benchmark_sub_a 		passed in:		1.152048			Throughput:		43400k
//benchmark_and_b 		passed in:		1.359530			Throughput:		36777k
//benchmark_or__b 		passed in:		1.915101			Throughput:		26108k
//benchmark_mul_a 		passed in:		1.291451			Throughput:		38716k
//benchmark_eq__b 		passed in:		8.453338			Throughput:		5914k
//benchmark_neq_b 		passed in:		8.163076			Throughput:		6125k
//benchmark_gt__b 		passed in:		8.819247			Throughput:		5669k
//benchmark_lt__b 		passed in:		8.688485			Throughput:		5754k
//benchmark_gte_b 		passed in:		8.687986			Throughput:		5755k
//benchmark_lte_b 		passed in:		8.746762			Throughput:		5716k

// NO Random 3PC    500, 100k   MPI_parallelism_factor = 4
//benchmark_xor_b 		passed in:		1.369651			Throughput:		36505k
//benchmark_add_a 		passed in:		1.175563			Throughput:		42532k
//benchmark_sub_a 		passed in:		1.224116			Throughput:		40845k
//benchmark_and_b 		passed in:		1.410893			Throughput:		35438k
//benchmark_or__b 		passed in:		1.974350			Throughput:		25324k
//benchmark_mul_a 		passed in:		1.433421			Throughput:		34881k
//benchmark_eq__b 		passed in:		8.680755			Throughput:		5759k
//benchmark_neq_b 		passed in:		8.239879			Throughput:		6068k
//benchmark_gt__b 		passed in:		8.693289			Throughput:		5751k
//benchmark_lt__b 		passed in:		8.654262			Throughput:		5777k
//benchmark_gte_b 		passed in:		8.794166			Throughput:		5685k
//benchmark_lte_b 		passed in:		8.838303			Throughput:		5657k


// Random PRG 3PC    500, 100k   MPI_parallelism_factor = 1
//benchmark_xor_b 		passed in:		1.475383			Throughput:		33889k
//benchmark_add_a 		passed in:		1.220979			Throughput:		40950k
//benchmark_sub_a 		passed in:		1.177377			Throughput:		42467k
//benchmark_and_b 		passed in:		1.798747			Throughput:		27797k
//benchmark_or__b 		passed in:		2.365722			Throughput:		21135k
//benchmark_mul_a 		passed in:		1.642858			Throughput:		30434k
//benchmark_eq__b 		passed in:		10.171160			Throughput:		4915k
//benchmark_neq_b 		passed in:		10.741640			Throughput:		4654k
//benchmark_gt__b 		passed in:		10.325947			Throughput:		4842k
//benchmark_lt__b 		passed in:		10.261373			Throughput:		4872k
//benchmark_gte_b 		passed in:		10.374961			Throughput:		4819k
//benchmark_lte_b 		passed in:		10.386108			Throughput:		4814k


// ALL:
//benchmark_xor_b 		passed in:		1.313092			Throughput:		38078k
//benchmark_add_a 		passed in:		1.097301			Throughput:		45566k
//benchmark_sub_a 		passed in:		1.115461			Throughput:		44824k
//benchmark_and_b 		passed in:		1.655672			Throughput:		30199k
//benchmark_or__b 		passed in:		2.188436			Throughput:		22847k
//benchmark_mul_a 		passed in:		1.593948			Throughput:		31368k
//benchmark_eq__b 		passed in:		9.910336			Throughput:		5045k
//benchmark_neq_b 		passed in:		9.627974			Throughput:		5193k
//benchmark_gt__b 		passed in:		10.312520			Throughput:		4848k
//benchmark_lt__b 		passed in:		10.279699			Throughput:		4863k
//benchmark_gte_b 		passed in:		10.793941			Throughput:		4632k
//benchmark_lte_b 		passed in:		10.567654			Throughput:		4731k


// NONE
//benchmark_xor_b 		passed in:		1.038711			Throughput:		48136k
//benchmark_add_a 		passed in:		0.839542			Throughput:		59556k
//benchmark_sub_a 		passed in:		0.833371			Throughput:		59997k
//benchmark_and_b 		passed in:		1.050066			Throughput:		47616k
//benchmark_or__b 		passed in:		1.566116			Throughput:		31926k
//benchmark_mul_a 		passed in:		0.884309			Throughput:		56541k
//benchmark_eq__b 		passed in:		6.720453			Throughput:		7439k
//benchmark_neq_b 		passed in:		6.527836			Throughput:		7659k
//benchmark_gt__b 		passed in:		6.897359			Throughput:		7249k
//benchmark_lt__b 		passed in:		6.483095			Throughput:		7712k
//benchmark_gte_b 		passed in:		6.547021			Throughput:		7637k
//benchmark_lte_b 		passed in:		6.388842			Throughput:		7826k


// RANDOM
//benchmark_xor_b 		passed in:		1.235088			Throughput:		40482k
//benchmark_add_a 		passed in:		0.947676			Throughput:		52760k
//benchmark_sub_a 		passed in:		0.959516			Throughput:		52109k
//benchmark_and_b 		passed in:		1.369083			Throughput:		36520k
//benchmark_or__b 		passed in:		1.886443			Throughput:		26504k
//benchmark_mul_a 		passed in:		1.124713			Throughput:		44455k
//benchmark_eq__b 		passed in:		7.963701			Throughput:		6278k
//benchmark_neq_b 		passed in:		7.758939			Throughput:		6444k
//benchmark_gt__b 		passed in:		8.279555			Throughput:		6038k
//benchmark_lt__b 		passed in:		8.110702			Throughput:		6164k
//benchmark_gte_b 		passed in:		9.780853			Throughput:		5112k
//benchmark_lte_b 		passed in:		8.568467			Throughput:		5835k


// Communication
//benchmark_xor_b 		passed in:		1.284633			Throughput:		38921k
//benchmark_add_a 		passed in:		1.119920			Throughput:		44646k
//benchmark_sub_a 		passed in:		1.132003			Throughput:		44169k
//benchmark_and_b 		passed in:		1.347235			Throughput:		37113k
//benchmark_or__b 		passed in:		1.914353			Throughput:		26118k
//benchmark_mul_a 		passed in:		1.462403			Throughput:		34190k
//benchmark_eq__b 		passed in:		9.985408			Throughput:		5007k
//benchmark_neq_b 		passed in:		8.813385			Throughput:		5673k
//benchmark_gt__b 		passed in:		8.696196			Throughput:		5749k
//benchmark_lt__b 		passed in:		8.976873			Throughput:		5569k
//benchmark_gte_b 		passed in:		8.777385			Throughput:		5696k
//benchmark_lte_b 		passed in:		8.832261			Throughput:		5661k