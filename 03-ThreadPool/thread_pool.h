
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace thread_pool {
using Task = std::function<void()>;
using UniqueTask = std::unique_ptr<Task>;

struct StopException : public std::exception {};

struct ThreadPool final
{
    // DATA
    std::mutex mutex;                           // Mutex for incomming jobs
    std::condition_variable waiter;             // Notify incomming jobs
    std::condition_variable pool;               // Notify thread creation
    std::vector<std::thread> executors;         // Executor threads
    std::deque<UniqueTask> tasks;               // Task collection

    // CTOR
    ThreadPool(int count)
    {
        std::unique_lock<std::mutex> notif { mutex };
        for (; count > 0; --count) {
            executors.push_back(std::thread(&ThreadPool::execute_thread, this));
            pool.wait(notif);
        }
    }

    // DTOR
    ~ThreadPool()
    {
        std::vector<UniqueTask> tasks;
        tasks.reserve(executors.size());
        for (size_t i = 0; i < executors.size(); ++i)
            tasks.push_back(std::make_unique<Task>([]() { throw StopException(); } ));
        schedule(std::move(tasks));
        for (auto& executor : executors) {
            executor.join();
        }
        while (auto f = get_task()) {
            (*f)();
        }
    }

    // SCHEDULE
    template<typename C>
    void schedule(C&& coll)
    {
        std::lock_guard<std::mutex> lock { mutex };
        for (auto&& task : coll)
            tasks.push_back(std::move(task));
        waiter.notify_all();
    }
    template<>
    void schedule(UniqueTask&& task)
    {
        std::lock_guard<std::mutex> lock { mutex };
        tasks.push_back(std::move(task));
        waiter.notify_one();
    }

    // EXECUTE
    bool execute()
    {
        try {
            std::unique_lock<std::mutex> lock { mutex };
            if (auto t = get_task()) {
                lock.unlock();
                (*t)();
                return true;
            }
        } catch (const StopException&) {
            schedule(std::make_unique<Task>([]() { throw StopException(); } ));
        }
        return false;
    }
    // END

private:
    UniqueTask get_task()
    {
        if (tasks.empty())
            return UniqueTask();
        UniqueTask item { std::move(tasks.front()) };
        tasks.pop_front();
        return item;
    }

    // EXECUTE-THREAD
    void execute_thread()
    {
        std::unique_lock<std::mutex> notif { mutex };
        pool.notify_one();

        try {
            while (true) {
                waiter.wait(notif);
                while (auto task = get_task()) {
                    notif.unlock();
                    (*task)();
                    notif.lock();
                }
            }
        } catch (const std::exception&) {}
    }
    // END
};
} /* !namespace thread_pool */