// mpicxx -O3  -pthread -lsodium 3pc.cpp -o 3pc
// mpirun --bind-to none --mca btl_tcp_links 32 -np 3 ./3pc 1 384000 10000 1 0
// mpirun -host machine-1,machine-2,machine-3 --bind-to none --mca btl_tcp_links 32 -np 3 ./3pc 1 384000 10000 1 0


#include <vector>
#include <thread>
#include <pthread.h>
#include <memory>
#include <sys/time.h>

#include "mpi.h"

int rank;
int succ;
int pred;

void mult_exp(const int &batch_size,
              const int &thread_number) {

    // Seeds
    thread_local std::vector<unsigned short> local = {0,0,0};
    thread_local std::vector<unsigned short> remote = {0,0,0};

    // actual computation
    std::vector<int> x_local_ptr(batch_size);
    std::vector<int> x_remote_ptr(batch_size);

    std::vector<int> y_local_ptr(batch_size);
    std::vector<int> y_remote_ptr(batch_size);

    std::vector<int> res_local_ptr(batch_size);
    std::vector<int> res_remote_ptr(batch_size);

    // actual computation
    for(int i = 0; i < batch_size; ++i){
        res_local_ptr[i] =
                x_local_ptr[i] * y_local_ptr[i]
                + x_local_ptr[i] * y_remote_ptr[i]
                + x_remote_ptr[i] * y_local_ptr[i]
                + nrand48(&local[0]) - nrand48(&remote[0])
                ;
    }

    // Communication
    MPI_Request send_request;
    MPI_Request receive_request;

    MPI_Irecv(&res_remote_ptr[0], batch_size, MPI_INT32_T, succ, 700 + thread_number, MPI_COMM_WORLD,
              &receive_request);
    MPI_Isend(&res_local_ptr[0], batch_size, MPI_INT32_T, pred, 700 + thread_number, MPI_COMM_WORLD,
              &send_request);

    MPI_Wait(&receive_request, MPI_STATUS_IGNORE);
    MPI_Wait(&send_request, MPI_STATUS_IGNORE);
}

int main(int argc, char** argv){

    int provided;

    // MPI_Init(&argc, &argv);
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided != MPI_THREAD_MULTIPLE){
        printf("Sorry, this MPI implementation does not support multiple threads\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    succ = (rank - 1 + 3) % 3;
    pred = (rank + 1 + 3) % 3;

    int threads_number = 1;
    int exps_number = 5000;
    int batch_size = 8192;
    int total_processes_number = 1;
    int process_number = 0;


    if(argc >= 2 ){
        threads_number = atoi(argv[1]);
    }

    if(argc >= 3 ){
        exps_number = atoi(argv[2]);
    }

    if(argc >= 4 ){
        batch_size = atoi(argv[3]);
    }

    if(argc >= 5 ){
        total_processes_number = atoi(argv[4]);
    }

    if(argc >= 6 ){
        process_number = atoi(argv[5]);
    }


    std::string name = "benchmark_and_b";
    struct timeval begin, end;
    long seconds, micro;
    double elapsed;

    // start timer
    gettimeofday(&begin, 0);



    std::vector<std::thread> threads(threads_number);

    for(int thread_ind = 0; thread_ind < threads_number; ++thread_ind){
        int thread_ind_ = thread_ind;
        threads[thread_ind_] = std::thread([batch_size, exps_number, thread_ind_]() {
            for(int i = 0; i < exps_number; ++i){
                mult_exp(batch_size, thread_ind_);
            }
        });

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET((total_processes_number + process_number * threads_number + thread_ind_) % 48, &cpuset);
        int rc = pthread_setaffinity_np(threads[thread_ind_].native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
    }



    for(int thread_ind = 0; thread_ind < threads_number; ++thread_ind){
        threads[thread_ind].join();
    }

    if(rank == 0){
        // stop timer
        gettimeofday(&end, 0);
        seconds = end.tv_sec - begin.tv_sec;
        micro = end.tv_usec - begin.tv_usec;
        elapsed = seconds + micro * 1e-6;

        printf("%s \tpassed in:\t\t%f\t\t\tThroughput:\t\t%dk\n", name.c_str(), elapsed,
                int(exps_number * batch_size / 1000 * threads_number / elapsed ));
    }
    MPI_Finalize();
}