#pragma once

#include <algorithm>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace orq::libote_io {

/**
 * @brief Get or create a Boost.Asio io_context that is backed by `thread_cnt` worker threads.
 * @param thread_cnt The number of communication threads to use, must be specified on first call.
 *                   Subsequent calls may leave this parameter at 0.
 * @return A reference to the io_context.
 *
 * @note The first call initialises the context and starts the threads;
 * subsequent calls simply return the same context and ignore `thread_cnt`.
 */

class libOTeContext {
   public:
    // Access the singleton IO context directly
    static boost::asio::io_context& get_ioc(std::size_t thread_cnt = 0) {
        return getContext(thread_cnt).io();
    }

    // Delete copy/move
    libOTeContext(const libOTeContext&) = delete;
    libOTeContext& operator=(const libOTeContext&) = delete;

    // Singleton access
    static libOTeContext& getContext(std::size_t thread_cnt) {
        std::call_once(initFlag, [&]() {
            if (thread_cnt == 0) {
                throw std::runtime_error(
                    "libOTeContext::get_ioc: first call must specify thread count");
            }
            instance.reset(new libOTeContext(thread_cnt));
        });
        return *instance;
    }

    // Destructor stops the io_context and joins all threads
    ~libOTeContext() {
        guard.reset();  // allow io_context to exit when work is done
        ctx->stop();
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

   private:
    // Private constructor
    libOTeContext(std::size_t thread_cnt)
        : ctx(std::make_shared<boost::asio::io_context>()),
          guard(boost::asio::make_work_guard(*ctx)) {
        threads.reserve(thread_cnt);
        for (std::size_t i = 0; i < thread_cnt; ++i) {
            threads.emplace_back([ioc = ctx]() { ioc->run(); });
        }
    }

    boost::asio::io_context& io() { return *ctx; }

    std::shared_ptr<boost::asio::io_context> ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard;
    std::vector<std::thread> threads;

    static std::once_flag initFlag;
    static std::unique_ptr<libOTeContext> instance;
};

// Static member definitions (header-only)
inline std::once_flag libOTeContext::initFlag;
inline std::unique_ptr<libOTeContext> libOTeContext::instance = nullptr;

}  // namespace orq::libote_io
