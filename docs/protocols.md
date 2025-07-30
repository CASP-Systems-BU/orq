# Protocols
This documents describes what protocols are in secrecy and how to add a new protocol. When creating a new protocol, we think of it as an object per worker thread. It has enough information to execute some function in corresponding worker threads between different parties.

In 'Secrecy', a protocol is a set Multi-Party Computing functions that describe how to execute the following:
- How to generate new shares (both arthmetic and boolean) that will be distributed for the parties.
- How to reconstruct the data given the required shares.
- How a party can initiate a protocol with other computing parties to reconstruct the data from distributed secret shares between the parties.
- How to perform Addition and Multiplication for arithmetic shares (similarly XOR and AND for boolean shares).
- How to perfrom (negation for arithmetic shares) and (not and logical not for boolean shares).

## Implementing a new protocol
To implement a new protocol, we can execute the following steps; we show as an example a replicated semi-honest 3-parties protocol. The full code is under `core/protocols/replicated_3pc.h`.
- Create a new `.h` file with your a new class that represents your protocol.
- Make the class inherits from `protocol.h` class.
- The new protocol class should be a class template with the following types templates (check the computation library documentation for more information on how to use them):
```
template<typename Data, typename Share, typename Vector, typename SVector>
```
- Each protocol has two important objects that faciltates implemnting the protocol efficiently (`RandamoGenerator` and `Communicator`).
- As an example, about how initialize a protocol, check the following function:
```
Replicated_3PC(PartyID _partyID,
                Communicator *_communicator,
                RandomGenerator *_randomGenerator) :
                Protocol<Data, Share, Vector, SVector>(_communicator, _randomGenerator, _partyID, 3, 2) {}
```
The `_partyID` is simply the ID of the party running this secrecy service. The party ID can be useful in case of not symmetric protocol execution.
The `_communicator` and `_randomGenerator` are the instances of the `Communicator` and `RandomGenerator` created for this `Protocol` instance from the setup function as described in another section below.
The `Protocol` Base Class requires two more paramters which are the total number of parties for this protocol and the number of shares for each data element per party (i.e. Replication Factor in this case).
- The following is an example of how to implement the Multiplication function from the base class for this protocol.
```
// The inputs are `x` and `y` which are of type `SVector` that allows them to have multiple columns for replications if needed
// The output is also of same type as both `x` and `y`. Note that the `SVector` is a class template and therefore the function implementation should assume a specific type such `int` or `long long`. However, the `SVector` implementation allows it to have interface for the needed element wise CPU operations such as `+`, `*`, `^`, `!=`, ... etc.
SVector multiply_a(const SVector &x, const SVector &y) {

    long long size = x.size();

    // This is a use of the RandomGenerator
    // instance associated with this Protocol instance.
    // We instruct the random generator to fill the following
    // `Secrecy::Vector` named `r` from the 2nd channel.
    // The RandomGenerator is smart enough to detect the
    // type of the element used and fill the vector
    // accordingly.
    Vector r(size);
    this->randomGenerator->getMultipleNext(r, 2, size);

    // Local Computation
    // This line uses SVector abstraction such as that the
    // `()` operator is used to access columns.
    // Additionally, the `*` and `+` operators do element
    // wise CPU operations between `Secrecy::Vector` instances.
    auto local = (x(0) * y(0)) + (x(0) * y(1)) + (x(1) * y(0)) + r;

    // Communication Round
    // Similar to the `RandomGenerator` call, the communicator
    // call takes two `Secrecy::Vector`.
    // Additionally, it takes two important paramters `1` and `2`.
    // Those paramters represents to send data to a party that is +1 apart.
    // Also, it tells the communicator to take data from a party that is +2 apart.
    Vector remote(size);
    this->communicator->exchangeShares(local, remote, 1, 2, size);

    return std::vector<Vector>({local, remote});
}
```



## Protocol Setup 
In addition to Implement the protocol itself, we need to implement a setup function for this protocol. The setup function has the following functionalities:
- Defines which Communicator to use. Additionally, it provides the needed paramters to instantiate such communicators. For example, it can use the MPI Communicator.
- Defines which RandomGenerator to use and how to setup. For example, if we are to use a PRG, we need to specify it and also provides the needed seeds for the PRG.
- Defines the number of shares per element needed for each party.
- Additionally, it is responsible to parse the runtime configurationa and implements them such as (The number of threads, batch size ..).
- Note that each worker thread should have its own Communicator, RandomGenerator, and Protocol instance which are create in the setup function.
- The Communicators, RandomGenerators, and Protocols triplets are feeded into the Runtime.

For an example for a setup function, check the 
```
namespace secrecy{ namespace service { namespace mpi_service { namespace replicated_3pc{

    // This part simply Makes the data structures for the user more easily.
    // For example, they can then simply use `ASharedVector<int>`
    // whithout needing to respecify the number of columns for each shares.
    // Here we specify 4 things: the computation classes (Vector, SVector), Communicator,
    // RandomGenerator, Protocol including the replication number. 
    init_mpc_types(int, secrecy::Vector, std::vector<int>, secrecy::SVector, 2)
    init_mpc_system(secrecy::MPICommunicator, secrecy::PRGAlgorithm, secrecy::Replicated_3PC)
    template<typename T> using ASharedVector = secrecy::ASharedVector<T, secrecy::SVector<T,2>>;
    template<typename T> using BSharedVector = secrecy::BSharedVector<T, secrecy::SVector<T,2>>;

    // This is the setup function.
    static void secrecy_init(int argc, char** argv){
        int rank;

        // STEP One: Setup communication channels between parties.
#if defined(MPC_USE_MPI_COMMUNICATOR)
        int provided;

        // MPI_Init(&argc, &argv);
        MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
        if (provided != MPI_THREAD_MULTIPLE){
            printf("Sorry, this MPI implementation does not support multiple threads\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

        // STEP Two: Parse the arguments given to the executable
        // Set Batch Size / Number of Threads
        if(argc >= 2 ){
            runTime->get_num_threads() = atoi(argv[1]);
        }else{
            runTime->get_num_threads() = 1;
        }

        int p_factor;
        if (argc >= 3){
            p_factor = atoi(argv[2]);
        }else{
            p_factor = 1;
        }

        if (argc >= 4){
            runTime->batch_size = atoi(argv[3]);
        }else{
            runTime->batch_size = 10000;
        }

        // Step Three: The runtime is initialized and the needed threads are created.
        runTime->setup_threads();

        
        // Step Four: Create the triplets (Communicator, RandomGenerator, Protocol) instances for each thread
        // And Populate the runtime with such instances.
        for(int i = 0; i < runTime->get_num_threads(); ++i){
            runTime->communicators.push_back(std::unique_ptr<secrecy::Communicator>(
                    new Communicator(rank, 3, 700 + (i*10), p_factor)));

            // TODO: exchange PRG Seeds here and send to constructor
            runTime->randomGenerators.push_back(std::unique_ptr<secrecy::RandomGenerator>(
                    new RG(std::vector<unsigned short>({0, 0, 0}),
                           std::vector<unsigned short>({0, 0, 0}),
                           std::vector<unsigned short>({0, 0, 0}))));

            runTime->protocols_32.push_back(std::unique_ptr<secrecy::ProtocolBase>(
                    new Protocol(rank,
                                 runTime->communicators[i].get(),
                                 runTime->randomGenerators[i].get())));
        }

        AShares::protocol = (Protocol*)(runTime->protocols_32[0].get());
    }

} } } } // namespace secrecy::service::mpi_service::replicated_3pc
```
