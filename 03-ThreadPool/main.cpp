#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <deque>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <chrono>
#include <string>

static int global_id = 0;

const std::string get_id()
{
    thread_local const std::string id = std::string("Thread ").append(std::to_string(global_id++));
    return id;
}

void display(const std::string& val)
{
    std::string value = get_id();
    value.append(" ").append(val).append("\n");
    std::cout << value;
}

struct ThreadPool final
{
    using unique_void_function = std::unique_ptr<std::function<void()>>;
    std::mutex poolMutex;
    std::mutex collMutex;
    std::condition_variable waiter;
    std::vector<std::thread> executors;
    std::deque<unique_void_function> tasks;
    bool running = true;

    ThreadPool(int count)
    {
        for (; count > 0; --count) {
            executors.push_back(std::thread(&ThreadPool::execute, this, std::unique_lock<std::mutex>(poolMutex)));
        }
    }

    ~ThreadPool()
    {
        running = false;
        waiter.notify_all();
        for (auto& executor : executors)
            executor.join();
    }

    void execute(std::unique_lock<std::mutex>&& notif)
    {
        display("started");
        while (running) {
            waiter.wait(notif);
            notif.unlock();
            while (auto* f = get_task(notif)) {
                (*f)();
                delete f;
            }
        }
        display("stopped\n");
    }

    std::function<void()>* get_task(std::unique_lock<std::mutex>& notif)
    {
        std::lock_guard<std::mutex> lk(collMutex);
        if (tasks.empty()) {
            notif.lock();
            return nullptr;
        }
        auto* f = tasks.front().release();
        tasks.pop_front();
        return f;
    }

    template<typename T>
    void schedule(T&& coll)
    {
        std::lock_guard<std::mutex> lk(collMutex);
        for (auto& f : coll)
            tasks.push_back(std::make_unique<unique_void_function::element_type>(std::move(f)));
        waiter.notify_all();
    }
};

struct FusionTaskGenerator
{
    ThreadPool& thread_pool;
    int generator_count;
    FusionTaskGenerator(ThreadPool& pool, int generator_count)
        : thread_pool(pool), generator_count(generator_count) {}

    void execute()
    {
        int gc = generator_count;
        display(std::string("generating ").append(std::to_string(gc)));
        std::vector<std::function<void()>> generator;
        for (int i = 1; i < gc; ++i) {
            generator.push_back([=]() {
                FusionTaskGenerator(thread_pool, gc - 1).execute();
            });
        }
        thread_pool.schedule(generator);
    }
};

int main()
{
    std::cout << "ThreadPool\n";
    ThreadPool tp(5);
    auto f1 = std::function<void()>([]() { display("Display()"); });
    auto f2 = std::function<void()>([]() { display("Display2()"); });
    tp.schedule(std::vector<std::function<void()>>({ f1, f1, f2, f1, f2, f1 }));
    auto gen = std::function<void()>([&]() { FusionTaskGenerator(tp, 3).execute(); });
    tp.schedule(std::vector<std::function<void()>>({ gen }));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << std::endl;
    return 0;
}
