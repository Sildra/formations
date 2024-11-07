[Sommaire](../README.md)


# Duplication de vecteur

La duplication de vecteur peut être une opération coûteuse. Prenons l'exemple d'un vecteur dont les doublons sont filtrés.


```cpp
template<typename T>
static std::vector<T> filterVector(const std::vector<T>& source) {
    std::vector<T> result;

    for (const auto& data : source) {
        if (std::find(result.begin(), result.end(), data) == result.end())
            result.push_back(data);
    }

    return result;
}
```

Nous allons tenter de faire une version du filtre qui passe par un `set` ou un `unordered_set`. Les données sont copiées 2 fois et l'ordre n'est pas préservé dans ces tests.

```cpp
template<typename Result, typename T>
static Result filterSetToVector(const std::vector<T>& source) {
    Result result;

    for (const auto& data : source) {
        result.emplace(data);
    }

    return result;
}
```

L'utilisation d'un `shared_ptr` est relativement simple. Dans notre cas, nous allons juste mettre le résultat du filtre dans le test et faire une copie di `shared_ptr`.


Le template de test se présente sous cette forme.

```cpp
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
```

Pour facilement ajouter des données de tests, nous allons utiliser un générateur. Ce générateur utilise quelques principes de métaprogrammation pour notamment déduire le type de retour du générateur.

```cpp
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
```

Et pour nos tests, nous allons les jouer avec ces données.

```cpp
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
```


```bash
# $CC intersect.cpp -O3 -o intersect.exe
# intersect.exe
```

```bash
> $CC --std=c++17 -O3 vector_dup.cpp -o vector_dup.exe
```
|                                          | Set     | SetDup | Shared | Unordered | UnorderedDup | Vector  | VectorDup |
| ---------------------------------------- | ------- | ------ | ------ | --------- | ------------ | ------- | --------- |
| BigStr, S:    50, U:   24, D:   26       |   778ms~|  307ms |    1ms |     461ms |        356ms |   772ms~|     148ms |
| BigStr, S:    50, U:   36, D:   14       |   942ms~| 1100ms~|    1ms |     618ms~|       1186ms~|  1092ms~|     558ms~|
| BigStr, S:   100, U:   45, D:   55       |  1323ms~|  899ms~|    1ms |    1232ms~|        689ms~|  1919ms~|     266ms |
| BigStr, S:   500, U:  288, D:  212       | 10005ms~| 5806ms~|    1ms |    7133ms~|       6044ms~| 30722ms~|    2063ms~|
| basic_string, S:    50, U:   26, D:   24 |   606ms~|  182ms |    1ms |     565ms~|        184ms |   247ms |      11ms |
| basic_string, S:    50, U:   29, D:   21 |   653ms~|  209ms |    1ms |     574ms~|        195ms |   305ms |      13ms |
| basic_string, S:   100, U:   46, D:   54 |   914ms~|  652ms~|    1ms |     712ms~|        623ms~|   609ms~|      16ms |
| int, S:    30, U:    9, D:   21          |    74ms |   60ms |    1ms |      86ms |         67ms |    46ms |       5ms |
| int, S:    30, U:   30, D:    0          |   265ms |  214ms |    1ms |     216ms |        527ms~|    94ms |      12ms |
| int, S:   500, U:   97, D:  403          |  2158ms~| 1322ms~|    1ms |    1217ms~|       1231ms~|   298ms |       6ms |
| int, S:   500, U:  483, D:   17          |  8063ms~| 4309ms~|    1ms |    5534ms~|       4216ms~|  1331ms~|       7ms |
