#include "flyweight.h"

#include <string>
#include <vector>
#include <chrono>
#include <iostream>


static void showTime(const std::string& info, std::chrono::high_resolution_clock::time_point& start)
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << info << " executed in " << duration << "ms\n";
}


struct hash {
    template <class T> // from boost
    static void hash_combine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }
    size_t operator()(const std::vector<int>& v) const {
        std::size_t seed = 0;
        for (const auto& i : v)
            hash_combine(seed, i);
        return seed;
    }
};

int main() {
    flyweight::FlyWeight<std::vector<int>, hash> repo;
    size_t execCount = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 100000; ++i) {
        auto element = repo.get(std::vector<int> { rand(), rand(), rand() });
        for (size_t j = 0; j < 100000; ++j) {
            const std::vector<int>& data = element;
            ++execCount;
        }
    }
    showTime("FlyWeight execution", start);
    std::cout << execCount << std::endl;

    execCount = 0;
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 100; ++i) {
        auto element = std::make_shared<std::vector<int>>(std::vector<int> { rand(), rand(), rand() });
        for (size_t j = 0; j < 100000; ++j) {
            auto e2 = element;
            const std::vector<int>& data = *e2;
            ++execCount;
        }
    }
    showTime("shared_ptr execution", start);
    std::cout << execCount << std::endl;
}
