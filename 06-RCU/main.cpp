#include "rcu_atomic.h"
#include "rcu_shared.h"
#include "rcu_simple.h"
#include "rcu_spin.h"

#include <iostream>
#include <string>

#include "../T3-Bencher/bencher.h"

static const std::string parent_type { "Parent" };
static const std::string child_type { "Child" };

struct Parent {
    int a {};
    virtual ~Parent() = default;
    virtual const std::string& get_type() const { return parent_type; }
};
struct Child : public Parent {
    const std::string& get_type() const { return child_type; }
};

template<typename RCU>
struct Testing {
    static RCU rcu_bench;
    static void reader() {
        auto snapshot = rcu_bench.get_shared();
        volatile int a = snapshot->a;
    }

    static void writer() {
        rcu_bench.update([](const Parent& old) {
            auto copy = std::make_shared<Parent>(old);
            copy->a += 1;
            return copy;
        });
    }
};
using Simple = Testing<rcu_simple::RCU<Parent>>;
using Shared = Testing<rcu_shared::RCU<Parent>>;
using Atomic = Testing<rcu_atomic::RCU<Parent>>;
using Spin = Testing<rcu_spin::RCU<Parent>>;
template<> decltype(Simple::rcu_bench) Simple::rcu_bench { std::make_shared<Parent>() };
template<> decltype(Shared::rcu_bench) Shared::rcu_bench { std::make_shared<Parent>() };
template<> decltype(Atomic::rcu_bench) Atomic::rcu_bench { std::make_shared<Parent>() };
template<> decltype(Spin::rcu_bench) Spin::rcu_bench { std::make_shared<Parent>() };

// BENCH
using Bench = bencher::Bencher<bencher::ExecutorState<100'000>>;
static std::mutex mtx;
static std::condition_variable cv;
static std::atomic<int> finished {};
static std::atomic<bool> starvation = false;

void execute(Bench& bench, const std::string& test_name, int index, void(*executor)()) {
    bench.bench(test_name, std::to_string(index), [&](auto& state) {
            {
                std::unique_lock<std::mutex> lk { mtx };
                cv.wait(lk);
            }
            for (auto _ : state) {
                executor();
            }
            if (!starvation)
                return;
            ++finished;
            while (finished != 8)
                executor();
        });
}

void multi_executor(std::vector<bencher::ResultNode>& results, const std::string& test_name, const std::initializer_list<void(*)()>& functions) {
    std::cout << "Preparing test " << test_name << "\n";
    int index = 0;
    finished = 0;
    std::vector<std::pair<std::unique_ptr<Bench>, std::thread>> benchs;
    for (const auto& function : functions) {
        std::unique_ptr<Bench> b = std::make_unique<Bench>();
        benchs.emplace_back(std::make_pair(std::move(b), std::thread(execute, std::ref(*b), test_name, ++index, function)));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    cv.notify_all();
    for (auto& b : benchs) {
        b.second.join();
        for (const auto& r : b.first->get_results())
            results.push_back(r);
    }
}


int main() {

    std::vector<bencher::ResultNode> results;
    
    multi_executor(results, "Simple Re7 - Wr1", { Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::writer });
    multi_executor(results, "Simple Re4 - Wr4", { Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::writer, Simple::writer, Simple::writer, Simple::writer });
    multi_executor(results, "Simple Re1 - Wr7", { Simple::reader, Simple::writer, Simple::writer, Simple::writer, Simple::writer, Simple::writer, Simple::writer, Simple::writer });

    multi_executor(results, "Shared Re7 - Wr1", { Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::writer });
    multi_executor(results, "Shared Re4 - Wr4", { Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::writer, Shared::writer, Shared::writer, Shared::writer });
    multi_executor(results, "Shared Re1 - Wr7", { Shared::reader, Shared::writer, Shared::writer, Shared::writer, Shared::writer, Shared::writer, Shared::writer, Shared::writer });

    multi_executor(results, "Atomic Re7 - Wr1", { Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::writer });
    multi_executor(results, "Atomic Re4 - Wr4", { Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer });
    multi_executor(results, "Atomic Re1 - Wr7", { Atomic::reader, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer });
    
    multi_executor(results, "Spin Re7 - Wr1", { Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::writer });
    multi_executor(results, "Spin Re4 - Wr4", { Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::writer, Spin::writer, Spin::writer, Spin::writer });
    multi_executor(results, "Spin Re1 - Wr7", { Spin::reader, Spin::writer, Spin::writer, Spin::writer, Spin::writer, Spin::writer, Spin::writer, Spin::writer });
    
    starvation = true;

    multi_executor(results, "Simple Starved Re7 - Wr1", { Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::writer });
    multi_executor(results, "Simple Starved Re4 - Wr4", { Simple::reader, Simple::reader, Simple::reader, Simple::reader, Simple::writer, Simple::writer, Simple::writer, Simple::writer });
    multi_executor(results, "Simple Starved Re1 - Wr7", { Simple::reader, Simple::writer, Simple::writer, Simple::writer, Simple::writer, Simple::writer, Simple::writer, Simple::writer });

    multi_executor(results, "Shared Starved Re7 - Wr1", { Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::writer });
    multi_executor(results, "Shared Starved Re4 - Wr4", { Shared::reader, Shared::reader, Shared::reader, Shared::reader, Shared::writer, Shared::writer, Shared::writer, Shared::writer });
    multi_executor(results, "Shared Starved Re1 - Wr7", { Shared::reader, Shared::writer, Shared::writer, Shared::writer, Shared::writer, Shared::writer, Shared::writer, Shared::writer });

    multi_executor(results, "Atomic Starved Re7 - Wr1", { Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::writer });
    multi_executor(results, "Atomic Starved Re4 - Wr4", { Atomic::reader, Atomic::reader, Atomic::reader, Atomic::reader, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer });
    multi_executor(results, "Atomic Starved Re1 - Wr7", { Atomic::reader, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer, Atomic::writer });
    
    multi_executor(results, "Spin Starved Re7 - Wr1", { Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::writer });
    multi_executor(results, "Spin Starved Re4 - Wr4", { Spin::reader, Spin::reader, Spin::reader, Spin::reader, Spin::writer, Spin::writer, Spin::writer, Spin::writer });
    multi_executor(results, "Spin Starved Re1 - Wr7", { Spin::reader, Spin::writer, Spin::writer, Spin::writer, Spin::writer, Spin::writer, Spin::writer, Spin::writer });

    bencher::Formatter::Options display_options;
    display_options.sort_cols = true;
    bencher::Formatter::display(results, display_options);
    return 0;
}