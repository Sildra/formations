#pragma once

#include "meta.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

// JSON-SFINAE
namespace json {
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

// JSON-PAIR
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

// JSON-STRING
template<typename T>
struct cx<T, typename std::enable_if_t<is_basic_string_v<T>>>
{
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
}
// END
#endif /* !META_TEST */