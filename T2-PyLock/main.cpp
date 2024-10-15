#include "../03-ThreadPool/thread_pool.h"

void static_lock_function1(const std::function<void()>& executor) {
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    executor();
}

void static_lock_function2(const std::function<void()>& executor) {
    static std::mutex mutex2;
    std::lock_guard lock(mutex2);
    executor();
}

void static_lock_function1_void() {
    static_lock_function1([](){});
}

void static_lock_function2_void() {
    static_lock_function2([](){});
}

int main() {
    thread_pool::Task f1 = []() { static_lock_function1(static_lock_function2_void); };
    thread_pool::Task f2 = []() { static_lock_function2(static_lock_function1_void); };
    using namespace std::chrono_literals;
    thread_pool::Task f_sleep = []() { static_lock_function2([]() { std::this_thread::sleep_for(15ms); static_lock_function1_void(); }); };
    thread_pool::ThreadPool tp(10);
    tp.schedule(std::make_unique<thread_pool::Task>(f_sleep));
    std::vector<thread_pool::UniqueTask> displayTasks;
    for (auto& task : { f1, f1, f1, f2, f1, f2, f1, f1 })
        displayTasks.push_back(std::make_unique<thread_pool::Task>(task));
    tp.schedule(std::move(displayTasks));
    return 0;
}