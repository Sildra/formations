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
    // ENUM
    // TYPE
};
```

## Les concepts avancés

La grande particularité d'utiliser une enum qui contiens un type utilisateur est que ce type partage la mémoire avec les autres types. Dans ce cas, le constucteur de l'objet n'est pas appelé durant l'allocation.

```cpp
struct Variant {
    // CTOR
};
```

Dans la même idée, le destructeur de `std::string` n'est pas appelé à la destruction de l'union.

```cpp
struct Variant {
    // DTOR
};
```

Les Getters sont aussi le bienvenu pour accéder aux différentes variables de l'`enum`.

```cpp
struct Variant {
    // GET
};

Variant(42).get<int>();     // ERROR
Variant(42).get<int64_t>(); // OK
```


## Le test

```cpp
// TEST
```

```bash
> $CC -std=c++17 *.cpp -o variant.exe
> variant.exe
```