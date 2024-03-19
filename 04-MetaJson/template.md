# La Metaprogrammation

La metaprogrammation en C++ nous permet de spécialiser nos templates à l'aide de charactéristiques communes appelés `traits`.


La bibliothèque standard en contiens un certain nombre (`is_same`, `is_arithmetic`, ...) mais pour nos besoins, nous allons définir `is_string`, `is_pair` et `is_collection`.

Une lsite exhaustive de la bibliothèque standard est disponible sur le site [cppreference.com/meta](https://en.cppreference.com/w/cpp/meta)

## C++14

En C++11 et 14, la définition des `traits` passe par la définition de structures testant les caractéristiques recherchées à nos traits.

```cpp
// CPP14

> $CC -std=c++14 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
```
## C++17

En C++17, l'ajout de `std::void_t` permet de simplifier énormément la déclaration d'une expression.
Dans le cas où l'expression compile, le template est instancié, sinon il est ignoré.

```cpp
// CPP17

> $CC -std=c++17 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
```

## C++20

L'ajout des `concepts` en C++20 permet de simplifier grandement la définition de nos traits. Cependant la syntaxe des concepts est différente d'un C++ standard.

```cpp
// CPP20

> $CC -std=c++20 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
```

## Les collections

Pour les collections, nous verrifions que la structure contiens un `iterator`. Les collections user-defined sont ainsi gérées.

```cpp
// META-COLLECTION
```

## Les pairs

Les pairs sont des structures relativement simples et n'offrent que peu de prérequis. Dans notre cas, nous testons la présence d'un `first_value` dans la structure.

```cpp
// META-PAIR
```

## Les collections de pairs

Pour gérer pleinement nos futures classes JSON, nous allons dès à présent définir notre collection de pairs: une collection dont notre `iterator` est une `pair`.

```cpp
// META-PAIR_COLLECTION
```

## MetaTests

Les tests suivants permettent de rapidement savoir quel type sont compatibles avec les traits que nous venons de définir.

```cpp
// META-TEST
> meta.exe
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
// JSON-SFINAE
```

## Helper

Le comportement générique à l'utilisation de RapidJson dans les cas d'utilisation DOM est standard. Pour éviter de surcharger les templates, nous allons définir des helpers réalisant ces opérations.

```cpp
namespace json {
    // JSON-HELPERS
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
// JSON-STRING
/// Integers
// JSON-INTEGER
/// Float
// JSON-FLOAT
/// Bool
// JSON-BOOLEAN
```

## Les collections

TODO

```cpp
// JSON-COLLECTION
```

## Les pairs

TODO

```cpp
// JSON-PAIR
```

## JsonTests

Les tests json suivant verifient que la sérialisation puis la déserialisation d'un objet nous renvoient le même résultat.

```cpp
// JSON-TEST

> $CC -std=c++14 -DJSON_TEST -Irapidjson/include *.cpp -o json.exe
> json.exe
```

