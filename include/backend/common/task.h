#pragma once

#include <functional>

#ifndef MINIMUM_CHUNK_SIZE
/**
 * @brief Do not give any thread less than this amount of work (unless that's all that's left)
 *
 * Developers may specify at compile time, in which case this default is ignore.
 *
 * NOTE: this MUST be a multiple of 128 due to some internal alignment requirements.
 *
 */
#define MINIMUM_CHUNK_SIZE 256
#endif

static_assert(MINIMUM_CHUNK_SIZE % 128 == 0);

namespace orq::service {

namespace {
    /** @brief Compute the batch size for a given range and number of batches.
     * The function returns the computed batch size, which is a multiple of
     * MINIMUM_CHUNK_SIZE.
     *
     * Note: helper function to distribute each thread chunk into a number of
     * batches, which is used in the Task class.
     *
     * @param start The starting index of the range.
     * @param end The ending index of the range.
     * @param batches_num The number of batches (works for both negative and positive).
     * @return ssize_t The computed batch size.
     */
    ssize_t compute_equal_batches(size_t start, size_t end, ssize_t batches_num) {
        // negative batch size specifies number of chunks instead
        auto num_batches = std::abs(batches_num);
        // ceiling'd division into the correct number of chunks
        // must be a multiple of MINIMUM_CHUNK_SIZE... ceil division and
        // then scale back up
        auto num_chunks = (end - start + MINIMUM_CHUNK_SIZE - 1) / MINIMUM_CHUNK_SIZE;
        // again ceil - compute number of chunks per batch
        ssize_t batch_size = (num_chunks + num_batches - 1) / num_batches * MINIMUM_CHUNK_SIZE;
        return batch_size;
    }
}  // namespace

/**
 * A parent class representing the runtime's fundamental unit of single-threaded
 * computation. For a single call into the runtime (e.g. MPC functionality),
 * each worker gets one Task; a task is further divided into batches according
 * to the user-specified batch size.
 */
class Task {
   protected:
    // vector indices which this task operates on
    size_t start;
    size_t end;

    ssize_t batch_size;

   public:
    /**
     * @brief Construct a new Task object; if negative batch size (even
     * division), compute actual batch size for this worker.
     *
     * @param _start
     * @param _end
     * @param _batch_size
     */
    Task(size_t _start, size_t _end, ssize_t _batch_size)
        : start(_start), end(_end), batch_size(_batch_size) {
        if (_batch_size < 0) {
            batch_size = compute_equal_batches(_start, _end, _batch_size);
        }
    }

    virtual ~Task() {}

    /**
     * @brief Tasks will overload this function with their specific workload.
     * It will be called once per batch.
     */
    virtual void sub_execute(size_t, size_t) = 0;

    /**
     * @brief Execute a series of batched tasks using calls to sub_execute.
     *
     */
    void execute() {
        for (size_t i = start; i < end; i += batch_size) {
            size_t end_ = std::min(i + batch_size, end);
            sub_execute(i, end_);
        }
    }
};

/**
 * A Task which takes one input and returns via passed reference
 *
 * @tparam In The type of the input.
 * @tparam Ret The type of the result.
 */
template <typename In, typename Ret, typename... T>
class Task_1_ref : public Task {
    In x;
    Ret res;

    // the function to execute on a range of the input
    std::function<void(In &, Ret &)> func;

   public:
    Task_1_ref(const In &_x, Ret &_res, const size_t _start, const size_t _end,
               const ssize_t _batch_size, std::function<void(In &, Ret &)> _func)
        : Task(_start, _end, _batch_size), x(_x), res(_res), func(_func) {}

    void sub_execute(size_t start, size_t end) override {
        x.set_batch(start, end);
        res.set_batch(start, end);
        func(x, res);
    }
};

/**
 * A Task which takes two inputs and returns via passed reference
 *
 * @tparam In The type of both inputs.
 * @tparam Ret The type of the result.
 *
 * Identical to Task_1_ref but with 2 arguments to the function.
 */
template <typename In, typename Ret, typename... T>
class Task_2_ref : public Task {
    In x, y;
    Ret res;

    // the function to execute on a range of the input
    std::function<void(In &, In &, Ret &)> func;

   public:
    Task_2_ref(const In &_x, const In &_y, Ret &_res, const size_t _start, const size_t _end,
               const ssize_t _batch_size, std::function<void(In &, In &, Ret &)> _func)
        : Task(_start, _end, _batch_size), x(_x), y(_y), res(_res), func(_func) {}

    void sub_execute(size_t start, size_t end) override {
        x.set_batch(start, end);
        y.set_batch(start, end);
        res.set_batch(start, end);
        func(x, y, res);
    }
};

/**
 * A Task which takes two inputs and returns via passed reference, but aggregates
 * the results into a smaller output vector.
 *
 * @tparam In The type of both inputs.
 * @tparam Ret The type of the result.
 */
template <typename In, typename Ret, typename... T>
class Task_2_Agg_ref : public Task {
    In x, y;
    Ret res;
    const size_t agg_size;

    // the function to execute on a range of the input
    std::function<void(In &, In &, Ret &)> func;

   public:
    Task_2_Agg_ref(const In &_x, const In &_y, Ret &_res, const size_t _start, const size_t _end,
                   const ssize_t _batch_size, const size_t &_agg_size,
                   std::function<void(In &, In &, Ret &)> _func)
        : Task(_start, _end, _batch_size),
          x(_x),
          y(_y),
          res(_res),
          agg_size(_agg_size),
          func(_func) {}

    void sub_execute(size_t start, size_t end) override {
        x.set_batch(start, end);
        y.set_batch(start, end);
        res.set_batch(start / agg_size, end / agg_size);
        func(x, y, res);
    }
};

/**
 * A Task which takes zero inputs and returns void. This is used when maximum
 * flexibility is necessary; e.g. generating triples and
 * `execute_parallel_unsafe`.
 */
class Task_0_void : public Task {
    std::function<void(const size_t, const size_t)> func;

   public:
    Task_0_void(const size_t _start, const size_t _end, const ssize_t _batch_size,
                std::function<void(const size_t, const size_t)> _func)
        : Task(_start, _end, _batch_size), func(_func) {}

    void sub_execute(size_t start, size_t end) override { func(start, end); }
};

/**
 * A Task which takes one input and returns nothing.
 *
 * @tparam In The type of the input.
 */
template <typename In>
class Task_1_void : public Task {
    In x;

    // the function to execute on a range of the input
    std::function<void(In &)> func;

   public:
    Task_1_void(const In &_x, const size_t _start, const size_t _end, const ssize_t _batch_size,
                std::function<void(In &)> _func)
        : Task(_start, _end, _batch_size), x(_x), func(_func) {}

    void sub_execute(size_t start, size_t end) override {
        x.set_batch(start, end);
        func(x);
    }
};

/**
 * A Task which takes one input and returns nothing, and also has no batching.
 * Currently used for permutation generation.
 *
 * @tparam In The type of the input.
 */
template <typename In>
class Task_1_void_nobatch : public Task {
    In x;

    // the function to execute on a range of the input
    std::function<void(In &)> func;

   public:
    // Give superclass constructor fake batch size so it only runs a single
    // iteration.
    Task_1_void_nobatch(const In &_x, std::function<void(In &)> _func)
        : Task(0, 1, 1), x(_x), func(_func) {}

    void sub_execute(size_t start, size_t end) override { func(x); }
};

/**
 * A Task which takes 1 input and returns a pair.
 *
 * @tparam In The type of the input.
 * @tparam Ret The type of each result.
 */
template <typename In, typename Ret, typename... T>
class Task_1_pair : public Task {
    In x;
    std::pair<Ret, Ret> r;

    // the function to execute on a range of the input
    std::function<void(In &, Ret &, Ret &)> func;

   public:
    Task_1_pair(const In &_x, std::pair<Ret, Ret> &_r, const size_t _start, const size_t _end,
                const ssize_t _batch_size, std::function<void(In &, Ret &, Ret &)> _func)
        : Task(_start, _end, _batch_size), x(_x), r(_r), func(_func) {}

    /**
     * The main function which executes the task's function in batches.
     */
    void sub_execute(size_t start, size_t end) override {
        x.set_batch(start, end);
        r.first.set_batch(start, end);
        r.second.set_batch(start, end);
        func(x, r.first, r.second);
    }
};
/**
 * A Task which takes two inputs and returns nothing
 *
 * @tparam In The type of both inputs.
 *
 * Identical to Task_ARGS_VOID_1 but with 2 arguments to the function.
 */
template <typename In>
class Task_2_void : public Task {
    In x, y;

    // the function to execute on a range of the input
    std::function<bool(In &, In &)> func;

   public:
    Task_2_void(const In &_x, const In &_y, const size_t _start, const size_t _end,
                const ssize_t _batch_size, std::function<bool(In &, In &)> _func)
        : Task(_start, _end, _batch_size), x(_x), y(_y), func(_func) {}

    /**
     * The main function which executes the task's function in batches.
     */
    void sub_execute(size_t start, size_t end) override {
        x.set_batch(start, end);
        y.set_batch(start, end);
        func(x, y);
    }
};

}  // namespace orq::service
