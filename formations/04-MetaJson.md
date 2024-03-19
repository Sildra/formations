[Sommaire](../README.md)

# La Metaprogrammation

La metaprogrammation en C++ nous permet de spécialiser nos templates à l'aide de charactéristiques communes appelés `traits`.


La bibliothèque standard en contiens un certain nombre (`is_same`, `is_arithmetic`, ...) mais pour nos besoins, nous allons définir `is_string`, `is_pair` et `is_collection`.

Une lsite exhaustive de la bibliothèque standard est disponible sur le site [cppreference.com/meta](https://en.cppreference.com/w/cpp/meta)

## C++14

En C++11 et 14, la définition des `traits` passe par la définition de structures testant les caractéristiques recherchées à nos traits.

```cpp
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

> $CC -std=c++14 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
./meta.h:6:10: warning: C++14 [-W#pragma-messages]
```
## C++17

En C++17, l'ajout de `std::void_t` permet de simplifier énormément la déclaration d'une expression.
Dans le cas où l'expression compile, le template est instancié, sinon il est ignoré.

```cpp
template<typename T, typename = void>
static constexpr bool is_basic_string_v = false;
template<typename T>
static constexpr bool is_basic_string_v<T, std::void_t<decltype(std::declval<T>().c_str())>> = true;

> $CC -std=c++17 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
./meta.h:22:10: warning: C++17 [-W#pragma-messages]
```

## C++20

L'ajout des `concepts` en C++20 permet de simplifier grandement la définition de nos traits. Cependant la syntaxe des concepts est différente d'un C++ standard.

```cpp
template<typename T>
concept is_basic_string_v = requires(T a) { a.c_str(); }

> $CC -std=c++20 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
./meta.h:30:10: warning: C++20 [-W#pragma-messages]
```

## Les collections

Pour les collections, nous verrifions que la structure contiens un `iterator`. Les collections user-defined sont ainsi gérées.

```cpp
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
```

## Les pairs

Les pairs sont des structures relativement simples et n'offrent que peu de prérequis. Dans notre cas, nous testons la présence d'un `first_value` dans la structure.

```cpp
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
```

## Les collections de pairs

Pour gérer pleinement nos futures classes JSON, nous allons dès à présent définir notre collection de pairs: une collection dont notre `iterator` est une `pair`.

```cpp
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
```

## MetaTests

Les tests suivants permettent de rapidement savoir quel type sont compatibles avec les traits que nous venons de définir.

```cpp
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
> meta.exe
META:
Type                            String  Pair    Coll    PairColl
int                             false   false   false   false
char*                           false   false   false   false
std::string                     true    false   true    false
std::pair<int, char*>           false   true    false   false
std::vector<int>                false   false   true    false
std::map<std::string, int>      false   false   true    true
```

# Le JSON avec RapidJson

RapidJson est une bibiliothèque déveoppée par Tencent qui permet de manipuler des structures de données JSON en C++.
La priincipale problématique de cette bibliothèque est qu'elle commence à être un peu datée et n'intègre pas des concepts modernes de la metaprogrammation en C++.
De ce fait, la bibliothèque ne gère pas la sérialisation/déserialisation des collections de la STL out of the box.

Nos nouvelles connaissances sur la métaprogrammation peuvent nous permettre d'ajouter facilement ces capacités.

## SFINAE

Le SFINAE est une technique de metaprogrammation permetant d'établir la  base de notre template.
Dans notre cas, il nous permet de 3 avantages :

* Définir l'interface de base de notre template;
* Lever explicitement une erreur en cas d'instanciation;
* Réduire le nombre d'erreurs de compilations liées à l'instanciation d'un mauvais template

```cpp
namespace json {
template<typename T, typename Enable = void> struct cx
{
    static_assert(std::is_same<T, void>::value, "Class is not elligible for json conversion");
    static inline T deserialize(rapidjson::Value& v) { return T(); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f) {}
};

```

## Helper

Le comportement générique à l'utilisation de RapidJson dans les cas d'utilisation DOM est standard. Pour éviter de surcharger les templates, nous allons définir des helpers réalisant ces opérations.

```cpp
namespace json {
    template<typename T>
    static inline T deserialize(const std::string& json)
    {
        rapidjson::Document d;
        d.Parse(json.c_str());
        return cx<T>::deserialize(d);
    }
    
    template<typename T>
    static inline std::string serialize(const T& field)
    {
        rapidjson::Document d;
        cx<T>::serialize(d, d.GetAllocator(), field);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        d.Accept(writer);
        return buffer.GetString();
    }
    
}

/// Usage:
std::vector<int> v { 1, 3, 5 };
decltype(v) v2 = json::deserialize<decltype(v)>(json::serialize(v));
// v == v2
```

## Les primitives

Concernant RapidJson, les primitives qui sont gérées sont : { `String`, `Bool`, `Int`, `Int64`, `UInt`, `UInt64`, `Double` }. Nos allons nous limiter à `double` et `int64_t` dans notre exemple.

```cpp
/// String
template<typename T>
struct cx<T, typename std::enable_if_t<is_basic_string_v<T>>>
{
    static inline T deserialize(rapidjson::Value& v)
    { return v.GetString(); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = rapidjson::StringRef(f.c_str(), f.size()); }
};

/// Integers
template<typename T>
struct cx<T, typename std::enable_if_t<std::is_integral<T>::value>>
{
    static inline T deserialize(rapidjson::Value& v)
    { return static_cast<T>(v.GetInt64()); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = f; }
};

/// Float
template<typename T>
struct cx<T, typename std::enable_if_t<std::is_floating_point<T>::value>>
{
    static inline T deserialize(rapidjson::Value& v)
    { return static_cast<T>(v.GetDouble()); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = f; }
};
/// Bool
template<>
struct cx<bool>
{
    typedef bool T;
    static inline T deserialize(rapidjson::Value& v)
    { return v.GetBool(); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = f; }
};

```

## Les collections

TODO

```cpp
template<typename T>
struct cx<T, typename std::enable_if_t<is_collection_v<T> && !is_basic_string_v<T>>>
{
    typedef typename T::value_type value_type;
    static inline T deserialize(rapidjson::Value& v) {
        T collection;
        auto inserter = std::inserter(collection, collection.end());
        for (auto& v_ : v.GetArray()) {
            inserter = cx<value_type>::deserialize(v_);
        }
        return collection;
    }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    {
        d.SetArray();
        for (const auto& val : f) {
            rapidjson::Value v;
            cx<value_type>::serialize(v, a, val);
            d.PushBack(v, a);
        }
    }
};

```

## Les pairs

TODO

```cpp
template<typename T>
struct cx<T, typename std::enable_if_t<is_pair_v<T> && is_basic_string_v<typename T::first_type>>>
{
    typedef std::decay_t<typename T::first_type> first_type;
    typedef std::decay_t<typename T::second_type> second_type;
    static inline T deserialize(rapidjson::Value& v)
    {
        auto& val = *v.MemberBegin();
        return std::make_pair(cx<first_type>::deserialize(val.name), cx<second_type>::deserialize(val.value));
    }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    {
        d.SetObject();
        rapidjson::Value k;
        rapidjson::Value v;
        cx<first_type>::serialize(k, a, f.first);
        cx<second_type>::serialize(v, a, f.second);
        d.AddMember(k, v, a);
    }
};
template<typename T>
struct cx<T, typename std::enable_if_t<is_pair_v<T> && !is_basic_string_v<typename T::first_type>>>
{
    typedef std::decay_t<typename T::first_type> first_type;
    typedef std::decay_t<typename T::second_type> second_type;
    static inline T deserialize(rapidjson::Value& v)
    {
        return std::make_pair(
            cx<first_type>::deserialize(v.FindMember("Key")->value),
             cx<second_type>::deserialize(v.FindMember("Value")->value));
    }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    {
        d.SetObject();
        rapidjson::Value v;
        cx<first_type>::serialize(v, a, f.first);
        d.AddMember("Key", v, a);
        cx<second_type>::serialize(v, a, f.second);
        d.AddMember("Value", v, a);
    }
};

```

## JsonTests

Les tests json suivant verifient que la sérialisation puis la déserialisation d'un objet nous renvoient le même résultat.

```cpp
template<typename T>
void json_test(std::string description, const T& value)
{
    std::string json_str = json::serialize(value);
    T json_val = json::deserialize<T>(json_str);
    std::cout << description << ":\n"
        "\t" << json_str << "\n"
        "\t" "Same? " << std::boolalpha << (value == json_val) << "\n";
}

void test_json()
{
    std::cout << "JSON:\n";
    json_test("std::vector<int>", std::vector<int>{ 43, 55 });
    json_test("std::map<std::string, int>", std::map<std::string, int>{ { "Val1", 43 }, { "Val2", 55 } });
    json_test("std::map<int, std::string>", std::map<int, std::string>{ { 1, "43" }, { 2, "55" } });
    json_test("std::map<std::vector<int>, std::vector<std::string>>",
        std::map<std::vector<int>, std::vector<std::string>>
        { { { 0, 1, 2 }, { "A", "B", "C" } } ,
          { { 42 }, { "Universe" } },
        });
}

> $CC -std=c++14 -DJSON_TEST -Irapidjson/include *.cpp -o json.exe
> json.exe
JSON:
std::vector<int>:
        [43,55]
        Same? true
std::map<std::string, int>:
        [{"Val1":43},{"Val2":55}]
        Same? true
std::map<int, std::string>:
        [{"Key":1,"Value":"43"},{"Key":2,"Value":"55"}]
        Same? true
std::map<std::vector<int>, std::vector<std::string>>:
        [{"Key":[0,1,2],"Value":["A","B","C"]},{"Key":[42],"Value":["Universe"]}]
        Same? true
```

