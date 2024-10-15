#pragma once

#include "meta.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace json {
// JSON-SFINAE
template<typename T, typename Enable = void> struct cx
{
    static_assert(std::is_same<T, void>::value, "Class is not elligible for json conversion");
    static inline T deserialize(rapidjson::Value& v) { return T(); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f) {}
};

// JSON-HELPERS
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

// JSON-COLLECTION
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

// JSON-META-HAS_STRING_AFFINITY
template<typename T>
struct has_string_affinity {
    template<typename A>
    static constexpr bool test(A* pt, decltype(A::has_string_affinity)* pi = nullptr) {
        return A::has_string_affinity;
    }
    template<typename A>
    static constexpr bool test(...) { return false; }
    static constexpr bool value = test<typename std::decay<T>::type>(nullptr);
};
template<typename T>
static constexpr bool has_string_affinity_v = has_string_affinity<T>::value;

// JSON-PAIR
template<typename T>
struct cx<T, typename std::enable_if_t<is_pair_v<T> && has_string_affinity_v<json::cx<std::decay_t<typename T::first_type>>>>>
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
struct cx<T, typename std::enable_if_t<is_pair_v<T> && !has_string_affinity_v<json::cx<std::decay_t<typename T::first_type>>>>>
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

// JSON-STRING
template<typename T>
struct cx<T, typename std::enable_if_t<is_basic_string_v<T>>>
{
    static constexpr bool has_string_affinity = true;
    static inline T deserialize(rapidjson::Value& v)
    { return v.GetString(); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = rapidjson::StringRef(f.c_str(), f.size()); }
};

// JSON-BOOLEAN
template<>
struct cx<bool>
{
    typedef bool T;
    static inline T deserialize(rapidjson::Value& v)
    { return v.GetBool(); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = f; }
};

// JSON-INTEGER
template<typename T>
struct cx<T, typename std::enable_if_t<std::is_integral<T>::value>>
{
    static inline T deserialize(rapidjson::Value& v)
    { return static_cast<T>(v.GetInt64()); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = f; }
};

// JSON-FLOAT
template<typename T>
struct cx<T, typename std::enable_if_t<std::is_floating_point<T>::value>>
{
    static inline T deserialize(rapidjson::Value& v)
    { return static_cast<T>(v.GetDouble()); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { d = f; }
};
// END
} /* !namespace json */

#ifdef JSON_TEST
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

// JSON-TEST_ENUM
enum class StringTest { A, B };
template<>
struct json::cx<StringTest> {
    static constexpr bool has_string_affinity = true;
    typedef StringTest T;

    static const auto& to_string_mapper() {
        static const std::unordered_map<T, std::string> m = {
            { StringTest::A, "A" },
            { StringTest::B, "B" },
        };
        return m;
    } 
    static const auto& from_string_mapper() {
        static const auto m = []() {
            std::unordered_map<std::string, T> r;
            for (const auto& m_ : to_string_mapper())
                r.insert( { m_.second, m_.first } );
            return r;
        }();
        return m;
    }
    static T map(const std::string& value) {
        const auto& m = from_string_mapper();
        auto f = m.find(value);
        return (f != m.end() ? f->second : m.begin()->second);
    }
    static const std::string& map(T value) {
        const auto& m = to_string_mapper();
        auto f = m.find(value);
        return (f != m.end() ? f->second : m.begin()->second);
    }

    static inline T deserialize(rapidjson::Value& v)
    { return map(v.GetString()); }
    static inline void serialize(rapidjson::Value& d, rapidjson::Document::AllocatorType& a, const T& f)
    { const auto& v = map(f); d = rapidjson::StringRef(v.c_str(), v.size()); }
};

// JSON-TEST
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

    json_test("std::map<StringTest, int>",
        std::map<StringTest, int> { { StringTest::A, 0 }, { StringTest::B, 1 } }
    );
}
// END
#endif /* !META_TEST */