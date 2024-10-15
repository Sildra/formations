# La Métaprogrammation

La métaprogrammation en C++ nous permet de spécialiser nos templates à l'aide de caractéristiques communes appelés `traits`.


La bibliothèque standard en contiens un certain nombre (`is_same`, `is_arithmetic`, ...) mais pour nos besoins, nous allons définir `is_string`, `is_pair` et `is_collection`.

Une liste exhaustive de la bibliothèque standard est disponible sur le site [cppreference.com/meta](https://en.cppreference.com/w/cpp/meta)

## C++14

En C++11 et 14, la définition des `traits` passe par la définition de structures testant les caractéristiques recherchées à nos traits.

```cpp
// CPP14
```

```bash
> $CC -std=c++14 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
```
## C++17

En C++17, l'ajout de `std::void_t` permet de simplifier énormément la déclaration d'une expression.
Dans le cas où l'expression compile, le template est instancié, sinon il est ignoré.

```cpp
// CPP17
```

```bash
> $CC -std=c++17 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
```

## C++20

L'ajout des `concepts` en C++20 permet de simplifier grandement la définition de nos traits. Cependant la syntaxe des concepts est différente d'un C++ standard.

```cpp
// CPP20
```

```bash
> $CC -std=c++20 -DMETA_TEST -Irapidjson/include *.cpp -o meta.exe 2>&1 | grep ' C++'
```

## Les collections

Pour les collections, nous vérifions que la structure contiens un `iterator`. Les collections user-defined sont ainsi gérées.

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

## Les smart pointers

L'ajout de smart pointers nous permet de distinguer un type standard d'un type managé. Les smart pointers ont en général une surcharge de l'operateur `->` et `*` qui leur permet un déréférencement automatique.


```cpp
// META-SMARTPTR
```

## MetaTests

Les tests suivants permettent de rapidement savoir quel type sont compatibles avec les traits que nous venons de définir.

```cpp
// META-TEST
```

```bash
> meta.exe
```

# Le JSON avec RapidJson

RapidJson est une bibliothèque développée par Tencent qui permet de manipuler des structures de données JSON en C++.
La principale problématique de cette bibliothèque est qu'elle commence à être un peu datée et n'intègre pas des concepts modernes de la métaprogrammation en C++.
De ce fait, la bibliothèque ne gère pas la sérialisation/désérialisation des collections de la STL out of the box.

Nos nouvelles connaissances sur la métaprogrammation peuvent nous permettre d'ajouter facilement ces capacités.

## SFINAE

Le SFINAE (Substitution Failure Is Not An Error) est une technique de métaprogrammation permettant d'établir la  base de notre template.
Le compilateur résous les templates et élimine silencieusement les surcharges dont les signatures produisent des erreurs de compilation.

Dans notre cas, nous définissons un template terminal qui nous permet de profiter 3 avantages :

* Définir l'interface de base de notre template et servir implicitement de documentation;
* Lever explicitement une (et une seule) erreur claire en cas d'instanciation;
* Réduire le nombre d'erreurs de compilations ou de suggestions liées à l'instanciation d'un mauvais template ou la mauvaise déduction de signatures

Dans notre cas, le template que nous définissons est valide en toutes circonstances mais est le moins contraint. Il n'est utilisé que si aucun autre template n'a pu être résolu sans erreurs.

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

Concernant RapidJson, les primitives qui sont gérées sont : { `String`, `Bool`, `Int`, `Int64`, `UInt`, `UInt64`, `Double` }. Nos allons nous limiter à `string`, `bool`, `double` et `int64_t` dans notre exemple.

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

En JSON les collections utilisent le même pattern : `[ $value, ...]`. Ce pattern s'applique aussi bien pour les objets à 1 élément (`std::set`, `std::vector`, ...) que ceux contenant des pairs (`std::map`, `std::multimap`, ...). Nous prenons avantage du `std::inserter` afin de normaliser l'interface d'insertion de nos collections.

```cpp
// JSON-COLLECTION
```

## Les pairs

L'utilité principale de notre `std::pair` est majoritairement de pouvoir traiter nos collections de pairs associatives de manière uniforme.

L'utilisation usuelle de cette association prends la forme `{ "$key": $value }` mais limite le format de la clé à une chaine de caractères. Une forme spécialisée est utilisée pour ce cas particulier, le cas générique utilisant la forme `{ "Key": $key, "Value": $value }`. Afin de mieux profiter du format court de la pair, nous utilisons un flag dans la structure qui pourra être définit dans les surcharges de templates.

Le trait de métaprogrammation est définit comme suit.

```cpp
// JSON-META-HAS_STRING_AFFINITY
```

Les structures json de la pair sont définies comme suit avec notre trait.

```cpp
// JSON-PAIR
```


## JsonTests

Afin de tester le comportement de notre affinité avec une `Enum`, nous définissons la structure de conversion suivante :

```cpp
// JSON-TEST_ENUM
```

Les tests json suivant vérifient que la sérialisation puis la désérialisation d'un objet nous renvoient le même résultat.

```cpp
// JSON-TEST
```

```bash
> $CC -std=c++14 -DJSON_TEST -Irapidjson/include *.cpp -o json.exe
> json.exe
```

