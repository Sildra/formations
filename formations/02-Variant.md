[Sommaire](../README.md)

# Le Variant

Le variant est un type de donnée qui n'est pas fixe au cours du temps. Ce type a été introduit avec `C++17` mais son mécanisme peut être adapté aux versions inférieures à l'aide d'une `enum` et d'une `union`.

On peut parler de polymorphisme statique en opposition au polymorphisme traditionnel et qui ont les propriétés suivantes :

* Polymorphisme statique :
  * Les types sont connus à la déclaration
  * Nécessite en général des templates afin de spécialiser des comportements
  * Peut utiliser un type pour définir des metadatas
  * Utilisable traditionnellement en `C` avec `void*` et un index 
  * Correspond aux surcharges de fonctions
* Polymorphisme dynamique :
  * Principe de l'héritage
  * Utilise une `vtable` pour référencer les spécialisations de comportement
  * Peut utiliser `dynamic_cast`
  

Le polymorphisme statique se traduit en général par des performances accrues notamment dans les cas où le compilateur inline les fonctions et améliore la localité de cache CPU.

Dans cet article nous présenterons un exemple contenant quelques types de base, une comparaison avec le `std::variant` et un cas d'utilisation possible dans une application open-source dans la bibliothèque d'accès à la base de données [Soci](htts://github.com/Soci/soci).


# Le code

## La structure de base

La structure basique de notre variant est :
* Une `enum`, dans notre cas: { `NONE`, `STRING`, `DOUBLE`, `INT64`, `BOOL` }
* Une `union` qui contiendra la valeur de notre `variant` ainsi que ses différents accesseurs.

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

La grande particularité d'utiliser une `enum` qui contiens un type utilisateur est que ce type partage la mémoire avec les autres types. Dans ce cas, le constructeur de l'objet n'est pas appelé durant l'allocation.

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

Dans notre cas, la spécialisation de l'operateur d'assignation permet d'optimiser les opérations de destruction et construction de `std::string`.

```cpp
struct Variant {
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

Pour tester notre variant, nous le comparons à un cas d'utilisation que nous aurions pu avoir face au standard.

Pour notre variant, nous utilisons l'operateur d'assignation avec un nouveau variant qui va se charger de supprimer pour nous l'instance précédente et pour le variant standard, nous utilisons une assignation de la nouvelle valeur.

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

La compilation de notre test et ses résultats se font comme suit :
* Fichier [main.cpp](https://github.com/Sildra/formations/tree/master/02-Variant/main.cpp)

```bash
> $CC -std=c++17 *.cpp -o variant.exe
> variant.exe
CustomVariant   stdVariant      Same?
42              42              true
TOTO            TOTO            true
TATA            TATA            true
40              40              true
```

## Cas d'utilisation Open-Source

Durant mes contributions à la bibliothèque d'accès à des bases de données [Soci](htts://github.com/Soci/soci), j'ai pu constater que l'implémentation des mappeurs de valeurs entre la base de données et le code client repose sur une structure virtuelle contenant un pointeur, le [type-holder.h (Soci v4.0.3)](https://github.com/SOCI/soci/blob/v4.0.3/include/soci/type-holder.h).

La construction de cet objet est systématiquement effectuée par une fonction template.

La récupération passe par un `dynamic_cast` et renvoie un `std::bad_cast` en cas d'échec de l'association.

Ce cas correspond à l'utilisation d'un `std::variant` et apporte les avantages suivants :
* Une standardisation de l'objet (mais nécessite `C++17`)
* Une suppression des opérations de `dynamic_cast`
* Une suppression des opérations de gestion mémoire liés aux pointeurs donc :
  * Une meilleure localité de cache
  * Une meilleure gestion de la mémoire de l'objet
  * Une meilleure conformité avec le principe [RAII (définition Wikipedia)](https://fr.wikipedia.org/wiki/Resource_acquisition_is_initialization)
