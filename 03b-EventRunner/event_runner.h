
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>

namespace event_runner {
struct IEvent { virtual ~IEvent() = default; };
using SharedEvent = std::shared_ptr<IEvent>;
using EventFunction = std::function<void(const IEvent&)>;

struct StopException : public std::exception {};

template<auto F>
struct ThreadPool final
{
    // DATA
    std::mutex mutex;                           // Mutex for incomming jobs
    std::condition_variable waiter;             // Notify incomming jobs
    std::condition_variable pool;               // Notify thread creation
    std::vector<std::thread> executors;         // Executor threads
    std::deque<SharedEvent> events;             // Event collection
    std::atomic_bool running = true;

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
        running = false;
        waiter.notify_all();
        for (auto& executor : executors) {
            executor.join();
        }
        while (auto event = get_event()) {
            F(*event);
        }
    }

    // SCHEDULE
    template<typename C>
    void schedule(C&& coll)
    {
        std::lock_guard<std::mutex> lock { mutex };
        for (auto& event : coll)
            events.push_back(event);
        waiter.notify_all();
    }
    
    void schedule(const SharedEvent& event)
    {
        std::lock_guard<std::mutex> lock { mutex };
        events.push_back(event);
        waiter.notify_one();
    }

    // EXECUTE
    bool execute()
    {
        std::unique_lock<std::mutex> lock { mutex };
        if (auto event = get_event()) {
            lock.unlock();
            F(event);
            return true;
        }
        return false;
    }
    // END

private:
    SharedEvent get_event()
    {
        if (events.empty())
            return SharedEvent();
        SharedEvent item { std::move(events.front()) };
        events.pop_front();
        return item;
    }

    // EXECUTE-THREAD
    void execute_thread()
    {
        std::unique_lock<std::mutex> notif { mutex };
        pool.notify_one();

        try {
            while (running) {
                waiter.wait(notif);
                while (auto event = get_event()) {
                    notif.unlock();
                    F(*event);
                    notif.lock();
                }
            }
            notif.unlock();
        } catch (const std::exception&) {}
    }
    // END
};
} /* !namespace event_runner */