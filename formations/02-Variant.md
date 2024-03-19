[Sommaire](../README.md)

# Le Variant

Le variant est un type de donnée qui n'est pas fixe au cours duu temps. Ce type a été introduit avec `C++17` mais son méchanisme peut être adapté aux versions inférieures à l'aide d'une `enum` et d'une `union`.

Dans cet article nous présenterons un exemple contenant quelques types de base, une comparaison avec le `std::variant` et un cas d'utilisation possible dans une application open-source.



# Le code

## La structure de base

La structure basique de notre variant est :
* Une `enum`, dans notre cas: { `NONE`, `STRING`, `DOUBLE`, `INT64`, `BOOL` }
* Une `union` qui contiens les valeurs 

```cpp
struct Variant {
    enum class Type { NONE, STRING, DOUBLE, INT64, BOOL };
    union VariantImpl {
        VariantImpl() : i(0) {}
        ~VariantImpl() {}
        std::string s;
        double d;
        int64_t i;
        bool b;
    } value;
    Type type;
};
```

## Les concepts avancés

La grande particularité d'utiliser une enum qui contiens un type utilisateur est que ce type partage la mémoire avec les autres types. Dans ce cas, le constucteur de l'objet n'est pas appelé durant l'allocation.

```cpp
struct Variant {
    Variant()                       : type(Type::NONE)   { }
    Variant(const std::string& val) : type(Type::STRING) { new (&value.s) std::string(val); }
    Variant(double val)             : type(Type::DOUBLE) { value.d = val; }
    Variant(int64_t val)            : type(Type::INT64)  { value.i = val; }
    Variant(bool val)               : type(Type::BOOL)   { value.b = val; }
    Variant(const void*) = delete;
};
```

Dans la même idée, le destructeur de `std::string` n'est pas appelé à la destruction de l'union.

```cpp
struct Variant {
    ~Variant() { if (type == Type::STRING) value.s.~basic_string(); }
};
```

Les Getters sont aussi le bienvenu pour accéder aux différentes variables de l'`enum`.

```cpp
struct Variant {
    template<typename T> const T& get() const
    { static_assert(std::is_same<T, void>::value); return T(); }
    template<> const std::string& get<std::string>() const { return value.s; }
    template<> const double&      get<double>()      const { return value.d; }
    template<> const int64_t&     get<int64_t>()     const { return value.i; }
    template<> const bool&        get<bool>()        const { return value.b; }
    
    Type getType() const { return type; }
};

Variant(42).get<int>();     // ERROR
Variant(42).get<int64_t>(); // OK
```


## Le test

```cpp

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
```

```bash
> $CC -std=c++17 *.cpp -o variant.exe
> variant.exe
CustomVariant   stdVariant      Same?
42              42              true
TOTO            TOTO            true
TATA            TATA            true
40              40              true
```