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

L'utilisation d'un `shared_ptr` est relativement simple. Dans notre cas, nous allons juste mettre le résultat du filtre dans le test et faire une copie de `shared_ptr`.


Le template de test se présente sous cette forme.

```cpp
template<typename T>
std::string left_pad(int size, T value) {
    std::string result(size, ' ');
    std::string v = std::to_string(value);
    return result.replace(std::max<size_t>(0, size - v.size()), v.size(), v);
}

std::string right_pad(int size, const std::string& value) {
    std::string result = value;
    result.resize(size, ' ');
    return result;
}
template<typename T>
std::string describe_test(const std::vector<T>& data) {
    auto uniques = filterVector(data).size();
    return right_pad(15, get_class_name<T>() + ",")
        + " S:  " + left_pad(4, data.size())
        + ", U: " + left_pad(4, uniques)
        + ", D: " + left_pad(4, data.size() - uniques);
}

template<typename Bencher, typename T>
void test(Bencher& benchmark, const std::vector<T>& data) {
    std::string testRow = describe_test(data);
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
    test(benchmark, create_data(500, [](){ return rand() % 100; }));
    test(benchmark, create_data(500, [](){ return rand() % 5000; }));
    bencher::Formatter::display(benchmark.get_results());

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
|                                            | Shared | Vector  | VectorDup | Set    | SetDup | Unordered | UnorderedDup |
| ------------------------------------------ | ------ | ------- | --------- | ------ | ------ | --------- | ------------ |
| basic_string,   S:    50, U:   26, D:   24 |    1ms |   232ms |      12ms |  577ms~|  180ms |     556ms~|        196ms |
| basic_string,   S:    50, U:   29, D:   21 |    1ms |   257ms |      12ms |  608ms~|  504ms |     551ms~|        196ms |
| basic_string,   S:   100, U:   46, D:   54 |    1ms |   640ms~|      16ms |  881ms~|  612ms~|     365ms |        606ms~|
| BigStr,         S:    50, U:   24, D:   26 |    1ms |   808ms~|     462ms |  756ms~|  900ms~|     459ms |        664ms~|
| BigStr,         S:    50, U:   36, D:   14 |    1ms |  1515ms~|     221ms |  921ms~| 1045ms~|     611ms~|       1138ms~|
| BigStr,         S:   100, U:   45, D:   55 |    1ms |  1918ms~|     264ms | 1286ms~|  874ms~|    1195ms~|        676ms~|
| BigStr,         S:   500, U:  288, D:  212 |    1ms | 32131ms~|    2003ms~| 9557ms~| 5295ms~|    7077ms~|       5581ms~|
| int,            S:    30, U:    9, D:   21 |    1ms |    46ms |       5ms |   80ms |   60ms |     413ms |         68ms |
| int,            S:    30, U:   30, D:    0 |    1ms |    77ms |       5ms |  216ms |  510ms |     522ms~|        194ms |
| int,            S:   500, U:   99, D:  401 |    1ms |   310ms |       6ms | 1974ms~| 1281ms~|    1520ms~|       1229ms~|
| int,            S:   500, U:  479, D:   21 |    1ms |  1323ms~|       7ms | 7835ms~| 4192ms~|    5870ms~|       4375ms~|
