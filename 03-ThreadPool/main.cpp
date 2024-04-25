#include <iostream>
#include <chrono>
#include <string>

#include "thread_pool.h"

// UTILS
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

void showTime(const std::string& info, std::chrono::high_resolution_clock::time_point& start)
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << info << " executed in " << duration << "ms\n";
}

// GENERATOR
struct TaskGenerator
{
    thread_pool::ThreadPool& thread_pool;
    int generator_count;
    TaskGenerator(thread_pool::ThreadPool& pool, int generator_count)
        : thread_pool(pool), generator_count(generator_count) {}

    void execute()
    {
        if (generator_count < 1)
            return;
        thread_pool::ThreadPool* tp = &thread_pool;
        int gc = generator_count - 1;
        std::vector<thread_pool::UniqueTask> generator;
        generator.reserve(gc);
        for (int i = 0; i < gc; ++i)
            generator.push_back(std::make_unique<thread_pool::Task>([=](){ TaskGenerator(*tp, gc).execute(); }));
        thread_pool.schedule(std::move(generator));
    }
};

// TESTS
int main()
{
    std::cout << "ThreadPool\n";
    {
        // TEST-CREATION
        auto now = std::chrono::high_resolution_clock::now();
        thread_pool::ThreadPool tp(5);
        showTime("ThreadPool creation", now);
        // TEST-DISPLAY
        using namespace std::chrono_literals;
        auto f1 = thread_pool::Task([]() { display("Display()"); std::this_thread::sleep_for(1ms); });
        auto f2 = thread_pool::Task([]() { display("Display2()"); std::this_thread::sleep_for(1ms); });
        std::vector<thread_pool::UniqueTask> displayTasks;
        for (auto& task : { f1, f1, f1, f2, f1, f2, f1, f1 })
            displayTasks.push_back(std::make_unique<thread_pool::Task>(task));
        tp.schedule(std::move(displayTasks));
        std::this_thread::sleep_for(15ms);
        // TEST-GENERATOR
        constexpr int operation_count = 10;
        now = std::chrono::high_resolution_clock::now();
        tp.schedule(std::make_unique<thread_pool::Task>([&]() { TaskGenerator(tp, operation_count).execute(); }));
        while (tp.execute())
            ;
        // Compute the number of operations
        int acc = 1;
        int tot = 1;
        for (int  i = operation_count - 1; i > 0; --i) {
            acc *= i;
            tot += acc;
        }
        showTime(std::to_string(tot) + " tasks", now);
        // END
    }
    std::cout << "End ThreadPool\n";
    return 0;
}
