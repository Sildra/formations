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
    // ENUM
    // TYPE
};
```

## Les concepts avancés

La grande particularité d'utiliser une `enum` qui contiens un type utilisateur est que ce type partage la mémoire avec les autres types. Dans ce cas, le constructeur de l'objet n'est pas appelé durant l'allocation.

```cpp
struct Variant {
    // CTOR
};
```

Dans notre cas, la spécialisation de l'operateur d'assignation permet d'optimiser les opérations de destruction et construction de `std::string`.

```cpp
struct Variant {
    // CTOR-EXTRA
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

Pour tester notre variant, nous le comparons à un cas d'utilisation que nous aurions pu avoir face au standard.

Pour notre variant, nous utilisons l'operateur d'assignation avec un nouveau variant qui va se charger de supprimer pour nous l'instance précédente et pour le variant standard, nous utilisons une assignation de la nouvelle valeur.

```cpp
// TEST
```

La compilation de notre test et ses résultats se font comme suit :
* Fichier [main.cpp](https://github.com/Sildra/formations/tree/master/02-Variant/main.cpp)

```bash
> $CC -std=c++17 *.cpp -o variant.exe
> variant.exe
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
