/**
 * @file worker.h
 * @brief A class which encapsulates a single worker thread
 *
 */

#include <barrier>
#include <condition_variable>
#include <thread>

#include "core/math/util.h"
#include "core/protocols/interface.h"
#include "core/random/manager.h"
#include "profiling/thread_profiling.h"
#include "task.h"
using namespace orq::instrumentation;

namespace orq::service {

class Worker {
   public:
    Worker(int rank, std::shared_ptr<std::barrier<>> barrier, WorkerConfig w)
        : terminate_(false),
          b_(barrier),
          m_(std::make_unique<std::mutex>()),
          cv_(std::make_unique<std::condition_variable>()),
          rank_(rank),
          wc(w) {
        thread_stopwatch::init_map(thread_.get_id());
    }

    // Default move constructor
    // (This is necessary because std::thread lives inside this object, and
    // cannot be copied.)
    Worker(Worker&&) = default;
    Worker& operator=(Worker&&) = default;

    // Copying not allowed!
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    ~Worker() {
        stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void start() { thread_ = std::thread(&Worker::run, this); }

    /**
     * @brief Called by the main thread to enqueue a task for this worker.
     * Once task is added, this worker is notified.
     *
     * @param t
     */
    void addTask(std::unique_ptr<Task>&& t) {
        {
            std::lock_guard<std::mutex> lock(*m_);
            q_.push(std::move(t));
        }
        cv_->notify_all();
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(*m_);
            terminate_ = true;
        }

        cv_->notify_all();
    }

    bool start_malicious_check() {
        bool ok = proto_8->start_malicious_check();
        ok &= proto_16->start_malicious_check();
        ok &= proto_32->start_malicious_check();
        ok &= proto_64->start_malicious_check();
        ok &= proto_128->start_malicious_check();
        return ok;
    }

    bool finalize_malicious_check() {
        bool ok = proto_8->finalize_malicious_check();
        ok &= proto_16->finalize_malicious_check();
        ok &= proto_32->finalize_malicious_check();
        ok &= proto_64->finalize_malicious_check();
        ok &= proto_128->finalize_malicious_check();
        return ok;
    }

    void reset_malicious_state() {
        proto_8->reset_malicious_state();
        proto_16->reset_malicious_state();
        proto_32->reset_malicious_state();
        proto_64->reset_malicious_state();
        proto_128->reset_malicious_state();
    }

    void print_statistics() {
        proto_8->print_statistics();
        proto_16->print_statistics();
        proto_32->print_statistics();
        proto_64->print_statistics();
        proto_128->print_statistics();
    }

    void mark_statistics() {
        proto_8->mark_statistics();
        proto_16->mark_statistics();
        proto_32->mark_statistics();
        proto_64->mark_statistics();
        proto_128->mark_statistics();
    }

    void clear_statistics() {
        proto_8->clear_statistics();
        proto_16->clear_statistics();
        proto_32->clear_statistics();
        proto_64->clear_statistics();
        proto_128->clear_statistics();
    }

    orq::random::RandomnessManager* getRandManager() const {
        assert(rand_ != nullptr);
        return this->rand_.get();
    }

    orq::Communicator* getCommunicator() const {
        assert(comm_ != nullptr);
        return this->comm_.get();
    }

    int getId() const { return wc.worker_id; }

    /**
     * @brief Return an instance of the protocol object specified by template argument
     *
     * @tparam T
     * @return std::unique_ptr<ProtocolBase>&
     */
    template <typename T>
    std::unique_ptr<ProtocolBase>& get_proto_instance() {
        if constexpr (std::is_same_v<T, int8_t>) {
            return proto_8;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            return proto_16;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return proto_32;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return proto_64;
        } else if constexpr (std::is_same_v<T, __int128_t>) {
            return proto_128;
        }
    }

    /**
     * @brief Get a pointer to the function-pointer member for a worker's protocol object specified
     * by the templated type.
     *
     * @tparam T
     * @return auto
     */
    template <typename T>
    static auto get_proto_member() {
        if constexpr (std::is_same_v<T, int8_t>) {
            return &Worker::proto_8;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            return &Worker::proto_16;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return &Worker::proto_32;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return &Worker::proto_64;
        } else if constexpr (std::is_same_v<T, __int128_t>) {
            return &Worker::proto_128;
        }
    }

    template <typename T, typename PF>
    void init_proto(PF& pf) {
        auto c = comm_.get();
        auto r = rand_.get();
        assert(c != nullptr && r != nullptr);

        auto p = pf.template create<T>(wc, c, r);
        // get_proto_instance returns a pointer reference, so this is legal
        get_proto_instance<T>() = std::move(p);
    }

    /**
     * @brief Attach a new communicator and randomness manager instance to this
     * worker.
     *
     * @param c
     * @param r
     */
    void attach(std::unique_ptr<Communicator>&& c,
                std::unique_ptr<orq::random::RandomnessManager>&& r) {
        assert(c != nullptr && r != nullptr);
        comm_ = std::move(c);
        rand_ = std::move(r);
    }

    /**
     * @brief Attach a new communicator instance to this worker. Use this method
     * when the randomness manager for the given protocol requires
     * communication to be instantiated first.
     *
     * @param c
     */
    void attach_comm(std::unique_ptr<Communicator>&& c) {
        assert(c != nullptr);
        comm_ = std::move(c);
    }

    /**
     * @brief Attach a new randomness manager to this worker. Use this method
     * when the randomness manager for the given protocol requires communication
     * to be instantiated first.
     *
     * @param r
     */
    void attach_rand(std::unique_ptr<orq::random::RandomnessManager>&& r) {
        assert(r != nullptr);
        rand_ = std::move(r);
    }

    // TODO: make this cleaner - singleton template types?
    // Ideally should be private, somehow, but Runtime needs access to
    // underlying protocol object.
    std::unique_ptr<ProtocolBase> proto_8;
    std::unique_ptr<ProtocolBase> proto_16;
    std::unique_ptr<ProtocolBase> proto_32;
    std::unique_ptr<ProtocolBase> proto_64;
    std::unique_ptr<ProtocolBase> proto_128;

   private:
    void run() {
#ifdef MPC_PROTOCOL_FANTASTIC_FOUR
        math::setup_NTL_4pc_hash_check();
#endif

        std::unique_ptr<Task> t;

        while (true) {
            // First iteration: wait for other threads to come up
            // After that: wait for last task to complete
            if (terminate_) {
                break;
            }

            b_->arrive_and_wait();

            {
                std::unique_lock<std::mutex> lock(*m_);

                // wait until a new task, or time to exit
                cv_->wait(lock, [this] { return !q_.empty() || terminate_; });

                if (terminate_) {
                    break;
                }

                t = std::move(q_.front());
                q_.pop();
            }

            // After a task has been pulled, release the lock (scoped ownership)
            // and then execute.

            {
                thread_stopwatch::InstrumentBlock _ib{};
                t->execute();
            }
        }
    }

    // This thread's communicator...
    std::unique_ptr<Communicator> comm_;
    // ...randomness manager...
    std::unique_ptr<orq::random::RandomnessManager> rand_;
    // ...and task queue.
    std::queue<std::unique_ptr<Task>> q_;

    // Party ID of this worker
    // All workers on one node have the same rank.
    int rank_;

    // synchronization stuff. Make these all pointers because the underlying
    // objects cannot be moved or copied.
    // Mutex protects condition variables. Barrier synchronizes workers with
    // each other and the main thread.
    std::unique_ptr<std::mutex> m_;
    std::unique_ptr<std::condition_variable> cv_;
    std::shared_ptr<std::barrier<>> b_;

    std::thread thread_;

    WorkerConfig wc;

    bool terminate_;
};

}  // namespace orq::service