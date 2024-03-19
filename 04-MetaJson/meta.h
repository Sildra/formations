#pragma once

#include <type_traits>

#if __cplusplus <= 201402L
# pragma message("C++14")
// CPP14
template<typename T>
struct is_basic_string {
    template<typename A>
    static constexpr bool test(A* pt, decltype(pt->c_str())* = nullptr) {
        return true;
    }
    template<typename A>
    static constexpr bool test(...) { return false; }
    static constexpr bool value = test<typename std::decay<T>::type>(nullptr);
};
template<typename T>
static constexpr bool is_basic_string_v = is_basic_string<T>::value;
// END
#elif __cplusplus <= 201703L
# pragma message("C++17")
// CPP17
template<typename T, typename = void>
static constexpr bool is_basic_string_v = false;
template<typename T>
static constexpr bool is_basic_string_v<T, std::void_t<decltype(std::declval<T>().c_str())>> = true;
// END
#else
# pragma message("C++20")
// CPP20
template<typename T>
concept is_basic_string_v = requires(T a) { a.c_str(); }
// END
#endif

// META-COLLECTION
template<typename T>
struct is_collection {
    template<typename A>
    static constexpr bool test(A* pt, typename A::iterator* = nullptr) {
        return true;
    }
    template<typename A>
    static constexpr bool test(...) { return false; }
    static constexpr bool value = test<typename std::decay<T>::type>(nullptr);
};
template<typename T>
static constexpr bool is_collection_v = is_collection<T>::value;
// META-PAIR
template<typename T>
struct is_pair {
    template<typename A>
    static constexpr bool test(A* pt, typename A::first_type* = nullptr) {
        return true;
    }
    template<typename A>
    static constexpr bool test(...) { return false; }
    static constexpr bool value = test<typename std::decay<T>::type>(nullptr);
};
template<typename T>
static constexpr bool is_pair_v = is_pair<T>::value;
// META-PAIR_COLLECTION
template<typename T>
struct is_pair_collection {
    template<typename A>
    static constexpr bool test(A* pt, typename A::iterator* pi = nullptr) {
        typedef typename A::value_type value_type;
        return std::is_same_v<decltype(**pi), value_type&> &&
            is_pair_v<value_type>;
    }
    template<typename A>
    static constexpr bool test(...) { return false; }
    static constexpr bool value = test<typename std::decay<T>::type>(nullptr);
};
template<typename T>
static constexpr bool is_pair_collection_v = is_pair_collection<T>::value;
// END
#ifdef META_TEST
#include <iostream>
#include <vector>
#include <map>
// META-TEST
template<typename T>
void meta(std::string description)
{
    description.resize(30, ' ');
    std::cout << description << std::boolalpha
        << "\t" << is_basic_string_v<T>
        << "\t" << is_pair_v<T>
        << "\t" << is_collection_v<T>
        << "\t" << is_pair_collection_v<T>
        << "\n";
}
#define META(...) meta<__VA_ARGS__>(#__VA_ARGS__)

void test_meta()
{
    std::cout << "META:\n"
        "Type\t\t\t\t" "String\t" "Pair\t" "Coll\t" "PairColl\n";
    META(int);
    META(char*);
    META(std::string);
    META(std::pair<int, char*>);
    META(std::vector<int>);
    META(std::map<std::string, int>);
}
// END
#endif /* !META_TEST */