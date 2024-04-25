#include <string>
#include <cstring>
#include <variant>

class Variant {
public:
    // ENUM
    enum class Type { NONE, STRING, DOUBLE, INT64, BOOL };
    // DTOR
    ~Variant() { if (type == Type::STRING) value.s.~basic_string(); }
    // CTOR
    Variant()                       : type(Type::NONE)   { }
    Variant(const std::string& val) : type(Type::STRING) { new (&value.s) std::string(val); }
    Variant(double val)             : type(Type::DOUBLE) { value.d = val; }
    Variant(int64_t val)            : type(Type::INT64)  { value.i = val; }
    Variant(bool val)               : type(Type::BOOL)   { value.b = val; }
    Variant(const void*) = delete;
    // CTOR-EXTRA
    Variant& operator=(Variant&& other) {
        if (this == &other)
            return *this;
        if (other.type == Type::STRING) { 
            if (type != Type::STRING)
                new (&value.s) std::string(std::move(other.value.s));
            else
                value.s = std::move(other.value.s);
            type = other.type;
        } else {
            this->~Variant();
            memcpy(&value, &other.value, sizeof(value));
        }
        return *this;
    }
    Variant(Variant&& other) { *this = std::move(other); }
    // GET
    template<typename T> const T& get() const
    { static_assert(std::is_same<T, void>::value); return T(); }
    template<> const std::string& get<std::string>() const { return value.s; }
    template<> const double&      get<double>()      const { return value.d; }
    template<> const int64_t&     get<int64_t>()     const { return value.i; }
    template<> const bool&        get<bool>()        const { return value.b; }

    Type getType() const { return type; }
    // END
private:
    // TYPE
    union VariantImpl {
        VariantImpl() : i(0) {}
        ~VariantImpl() {}
        std::string s;
        double d;
        int64_t i;
        bool b;
    } value;
    Type type;
    // END
}; /* !class Variant*/

#include <iostream>
#include <variant>

// TEST

using StdVariant = std::variant<std::monostate, std::string, int64_t, bool>;
template<typename T>
void print(const Variant& customVariant, const StdVariant& stdvariant) {
    std::cout << customVariant.get<T>()
        <<"\t\t" << std::get<T>(stdvariant) 
        << "\t\t" << (customVariant.get<T>() == std::get<T>(stdvariant))
        << "\n";
}

int main() {
    using namespace std::literals;
    std::cout << std::boolalpha << "CustomVariant\tstdVariant\tSame?\n";

    Variant customVariant = Variant((int64_t)42);
    StdVariant stdVariant((int64_t)42);
    print<int64_t>(customVariant, stdVariant);

    customVariant = Variant("TOTO"s);
    stdVariant = "TOTO"s;
    print<std::string>(customVariant, stdVariant);

    customVariant = Variant("TATA"s);
    stdVariant = "TATA"s;
    print<std::string>(customVariant, stdVariant);

    customVariant = Variant((int64_t)40);
    stdVariant = (int64_t)40;
    print<int64_t>(customVariant, stdVariant);
}
