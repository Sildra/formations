#include "bencher.h"

#include <unordered_set>
#include <set>
#include <vector>
#include <string>
#include <functional>
#include <memory>

#include "utils.h"

// FILTER_VECTOR
template<typename T>
static std::vector<T> filterVector(const std::vector<T>& source) {
    std::vector<T> result;

    for (const auto& data : source) {
        if (std::find(result.begin(), result.end(), data) == result.end())
            result.push_back(data);
    }

    return result;
}

// FILTER_SET_TO_VECTOR
template<typename Result, typename T>
static Result filterSetToVector(const std::vector<T>& source) {
    Result result;

    for (const auto& data : source) {
        result.emplace(data);
    }

    return result;
}

// VECTOR_DUP_TEST
template<typename T>
std::string left_pad(int size, T value) {
    std::string result(size, ' ');
    std::string v = std::to_string(value);
    return result.replace(std::max<size_t>(0, size - v.size()), v.size(), v);
}

template<typename Bencher, typename T>
void test(Bencher& benchmark, const std::vector<T>& data) {
    std::string testRow = [&]() {
            auto uniques = filterVector(data).size();
            return get_class_name<T>()
                + ", S:  " + left_pad(4, data.size())
                + ", U: " + left_pad(4, uniques)
                + ", D: " + left_pad(4, data.size() - uniques);
        }();
    benchmark.bench(testRow, "Shared", [&](auto& state) {
            auto shared = std::make_shared<std::vector<T>>(filterVector(data));
            for (auto _ : state) {
                auto filtered = shared;
                if (filtered->size() == 0)
                    return;
            }
        });
    benchmark.bench(testRow, "Vector", [&](auto& state) {
            for (auto _ : state) {
                auto filtered = filterVector(data);
                if (filtered.size() == 0)
                    return;
            }
        });
    benchmark.bench(testRow, "VectorDup", [&](auto& state) {
            auto filtered = filterVector(data);
            for (auto _ : state) {
                auto filtered2 = filtered;
                if (filtered2.size() == 0)
                    return;
            }
        });
    benchmark.bench(testRow, "Set", [&](auto& state) {
            for (auto _ : state) {
                auto filtered = filterSetToVector<std::set<T>>(data);
                if (filtered.size() == 0)
                    return;
            }
        });
    benchmark.bench(testRow, "SetDup", [&](auto& state) {
            auto filtered = filterSetToVector<std::set<T>>(data);
            for (auto _ : state) {
                auto filtered2 = filtered;
                if (filtered2.size() == 0)
                    return;
            }
        });
    benchmark.bench(testRow, "Unordered", [&](auto& state) {
            for (auto _ : state) {
                auto filtered = filterSetToVector<std::unordered_set<T>>(data);
                if (filtered.size() == 0)
                    return;
            }
        });
    benchmark.bench(testRow, "UnorderedDup", [&](auto& state) {
            auto filtered = filterSetToVector<std::unordered_set<T>>(data);
            for (auto _ : state) {
                auto filtered2 = filtered;
                if (filtered2.size() == 0)
                    return;
            }
        });
}

// VECTOR_DUP_BIG_STR
struct BigStr final {
    static std::string build(int range) {
        std::string source = "qazsedrftgyhujikolmpwxcvbn"; // Out of SSO
        source[range % source.size()] = 'A' + (range / source.size()) % 26;
        return source;
    }
    BigStr() = default;
    BigStr(int range) : value(build(range)) {}
    BigStr(const BigStr& other) = default;
    ~BigStr() = default;
    std::string value;
    bool operator==(const BigStr& other) const { return value == other.value; }
    bool operator<(const BigStr& other) const { return value < other.value; }
};

template<>
struct std::hash<BigStr> {
    size_t operator()(const BigStr& v) const { return std::hash<std::string>{}(v.value); }
};

// VECTOR_DUP_GEN
template<typename T, typename U = decltype(std::declval<T>()())>
std::vector<U> create_data(int size, T&& generator) {
    std::vector<U> result;
    for (int i = 0; i < size; ++i) {
        result.push_back(generator());
    }
    return result;
}

// Possible usage: create_data(500, [](){ return rand() % 100; });
// a vector<int> of 500 values that have at most 100 different values

// VECTOR_DUP_MAIN
int main() {
    bencher::Bencher<bencher::TimedExecutorState<100000, 500>> benchmark;
    test(benchmark, create_data(50, [](){ return std::string(1, 'a' + rand() % 26); }));
    test(benchmark, create_data(50, [](){ return std::string(1, 'a' + rand() % 52); }));
    test(benchmark, create_data(100, [](){ return std::string(1, 'a' + rand() % 52); }));
    test(benchmark, create_data(50, [](){ return BigStr(rand() % 26); }));
    test(benchmark, create_data(50, [](){ return BigStr(rand() % 52); }));
    test(benchmark, create_data(100, [](){ return BigStr(rand() % 52); }));
    test(benchmark, create_data(500, [](){ return BigStr(rand() % 400); }));
    test(benchmark, create_data(30, [](){ return rand() % 10; }));
    test(benchmark, create_data(30, [](){ return rand() % 5000; }));
    test(benchmark, create_data(500, [](){ return rand() % 5000; }));
    test(benchmark, create_data(500, [](){ return rand() % 100; }));
    benchmark.display();
    
    return 0;
}
