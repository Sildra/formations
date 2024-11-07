#include "bencher.h"

#include <set>
#include <unordered_set>
#include <vector>
#include <string>

#include "utils.h"

template<typename Container>
auto intersect(const Container& c1, const Container& c2) {
    int result = 0;
   // auto inserter = std::inserter(result, result.end());
    for (const auto& i : c1) {
        if (c2.count(i))
            ++result;
    }
    return result;
}

template<>
auto intersect(const std::vector<std::string>& c1, const std::vector<std::string>& c2) {
    int result = 0;
    //auto inserter = std::inserter(result, result.end());
    for (const auto& i : c1) {
        if (std::find(c2.begin(), c2.end(), i) != c2.end()) {
            ++result;
            // inserter = i;
        }
    }
    return result;
}

template<typename Container, typename Data, typename Bencher>
void benchSome(const Data& data, Bencher& benchmark) {
    benchmark.clear();
    Container c1;
    auto c1inserter = std::inserter(c1, c1.end());
    for (int i = 5; i < 15; ++i) {
        std::string i_name = (i < 10 ? " " : "") + std::to_string(i);

        c1inserter = data[i];
        Container c2;
        auto c2inserter = std::inserter(c2, c2.end());
        for (int j = (i / 2) + 1; j < i + 5; ++j) {
            auto index = j % data.size();
            std::string j_name = (index < 10 ? " " : "") + std::to_string(index);
            c2inserter = data[index];
            benchmark.bench(i_name, j_name, [&](auto& state) {
                for (auto _ : state) {
                    intersect(c1, c2);
                }
            });
        }
    }
    std::cout << "Results for " << get_class_name(c1) << "\n";
    benchmark.display();
}

void benchContainerIntersection() {
    bencher::Bencher<bencher::ExecutorState<1000000>> benchmark;
    std::vector<std::string> data;
    for (char c = 'A'; c <= 'Z'; ++c) {
        data.push_back(std::string(1, c));
    }
    benchSome<std::unordered_set<std::string>>(data, benchmark);
    benchSome<std::set<std::string>>(data, benchmark);
    bencher::Bencher<bencher::ExecutorState<1000000000>> vector_benchmark;
    benchSome<std::vector<std::string>>(data, vector_benchmark);
}

std::vector<std::string> createData() {
    static std::vector<std::string> initialData { "Data1", "Data2", "Data3", "Data1", "Data4" };
    std::vector<std::string> result;

    for (const auto& data : initialData) {
        if (std::find(result.begin(), result.end(), data) == result.end())
            result.push_back(data);
    }

    return result;
}

std::shared_ptr<std::vector<std::string>> getShared() {
    static std::shared_ptr<std::vector<std::string>> ptr = std::make_unique<std::vector<std::string>>(createData());
    return ptr;
}

void benchSharedPtrVsVectorDuplication() {
    bencher::Bencher<bencher::ExecutorState<10000000>> benchmark;
    benchmark.bench("Execution", "Shared", [](auto& state) {
            for (auto _ : state) {
                auto ptr = getShared();
                if (ptr->size() != 4)
                    return;
            }
        });
    benchmark.bench("Execution", "Copy", [](auto& state) {
            for (auto _ : state) {
                auto data = createData();
                if (data.size() != 4)
                    return;
            }
        });
    benchmark.display();
}

int main() {
    benchSharedPtrVsVectorDuplication();
}