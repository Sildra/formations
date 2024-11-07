#include <string>
#include <typeinfo>

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>

std::string demangle(const std::string& name) {
    int status = -4;
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name.c_str(), NULL, NULL, &status),
        std::free
    };

    return (status==0) ? res.get() : name ;
}
#else
std::string demangle(const std::string& name) { return name; }
#endif

template<typename T>
static inline std::string get_class_name() {
    std::string class_name = demangle(typeid(T).name());
    if (auto f = class_name.find('<'); f != std::string::npos)
        class_name.resize(f);
    if (auto f = class_name.rfind(':'); f != std::string::npos)
        class_name.erase(0, f + 1);
    else if (auto f = class_name.find("struct "); f == 0)
        class_name.erase(0, 7);
    else if (auto f = class_name.find("class "); f == 0)
        class_name.erase(0, 6);
    return class_name;
}
