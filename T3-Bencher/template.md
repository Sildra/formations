
# Duplication de structures de données

La duplication de structures de données peut être une opération coûteuse. Pour démontrer cet exemple nous allons utiliser le Bencher que nous venons de développer et tester des opérations de déduplication de données et de copie.


## Les données

Pour notre première version de déduplication, pour chaque donnée source nous cherchons si nous avons déjà ajouté cette donnée dans nos résultats. L'ordre des données uniques est ainsi préservée et le type de données initial est préservé.

```cpp
// FILTER_VECTOR
```

Nous allons faire une version du filtre qui passe par un `set` ou un `unordered_set`. Dans ces cas, l'ordre des données n'est pas préservée, et nous changeons le type de  retour par le type de la structure qui a servi à notre filtre.

```cpp
// FILTER_VECTOR_TO_SET
```

L'utilisation d'un `shared_ptr` est relativement simple. Dans notre cas, nous allons juste mettre le résultat du filtre dans le test et faire une copie de `shared_ptr`. Nous allons aussi faire une version thread-safe en wrappant le `shared_ptr` dans un `atomic` (c++20 uniquement).

## Les tests

Afin de mieux identifier nos résultats, nous décrivons nos tests à partir du type de la structure de données et du nombre de valeurs totales, uniques et en doublons de notre collection.

```cpp
// DATA_DUP_DESCRIBE
```

Pour nos tests, nous allons mesurer les performances suivantes :
* Copie d'un `shared_ptr` (cas de mise en cache vers une structure non-thread-safe)
* Copie d'un `atomic<shared_ptr>` (cas de mise en cache vers une structure thread-safe)
* Appel du dédoublonnement en passant par un `vector`
* Copie d'un `vector` (mise en cache du test précédent)
* Appel du dédoublonnement en passant par un `set`
* Copie d'un `set` (mise en cache du test précédent)
* Appel du dédoublonnement en passant par un `unordered_set`
* Copie d'un `unordered_set` (mise en cache du test précédent)

```cpp
// DATA_DUP_TEST
```

Pour facilement ajouter des données de tests, nous allons utiliser un générateur. Ce générateur utilise quelques principes de métaprogrammation pour notamment déduire le type de retour du générateur.

```cpp
// DATA_DUP_GEN
```
Et pour nos tests, nous allons tester ces structures de données :
* `std::string`: alloué avec un seul caractère, ce test minimise l'impact sur `vector` et `set`
* `BigStr`: une `std::string` dont la taille minimale est de 28 caractères et alloué dans un buffer externe
* `int`

```cpp
// DATA_DUP_MAIN
```


```bash
# $CC intersect.cpp -O3 -o intersect.exe
# intersect.exe
```

```bash
> $CC --std=c++20 -O3 vector_dup.cpp -o vector_dup.exe
```

Les résultats suivants ont été obtenus à l'aide de clang 14 sous WSL (c++17).

|                                            | Shared | Vector  | VectorCopy | Set    | SetCopy | Unordered | UnorderedCopy |
| ------------------------------------------ | ------ | ------- | ---------- | ------ | ------- | --------- | ------------- |
| basic_string,   S:    50, U:   21, D:   29 |    1ms |   190ms |        6ms |  253ms |    61ms |     249ms |          69ms |
| basic_string,   S:    50, U:   32, D:   18 |    1ms |   268ms |        8ms |  284ms |   102ms |     293ms |         118ms |
| basic_string,   S:   100, U:   41, D:   59 |    1ms |   678ms~|       14ms |  568ms~|   133ms |     546ms~|         157ms |
| BigStr,         S:    50, U:   22, D:   28 |    1ms |   402ms |       63ms |  512ms~|   124ms |     426ms |         139ms |
| BigStr,         S:    50, U:   33, D:   17 |    1ms |   750ms~|      129ms |  567ms~|   205ms |     480ms |         221ms |
| BigStr,         S:   100, U:   47, D:   53 |    1ms |  1353ms~|      214ms | 1166ms~|   293ms |     882ms~|         323ms |
| BigStr,         S:   500, U:  291, D:  209 |    1ms | 29958ms~|     1349ms~| 8700ms~|  2152ms~|    6238ms~|        2865ms~|
| int,            S:    30, U:   10, D:   20 |    1ms |    20ms |        1ms |   74ms |    33ms |      67ms |          29ms |
| int,            S:    30, U:   30, D:    0 |    1ms |    27ms |        1ms |  110ms |    84ms |     133ms |          88ms |
| int,            S:   500, U:  100, D:  400 |    1ms |   664ms~|        1ms | 1785ms~|   313ms |    1071ms~|         323ms |
| int,            S:   500, U:  480, D:   20 |    1ms |  3017ms~|        7ms | 3319ms~|  1696ms~|    3367ms~|        2423ms~|



Ceux-ci utilisent une version plus à jour de clang :

```bash
> $CC --version
```
|                                            | Shared | AtomicShared | Vector  | VectorCopy | Set     | SetCopy | Unordered | UnorderedCopy |
| ------------------------------------------ | ------ | ------------ | ------- | ---------- | ------- | ------- | --------- | ------------- |
| basic_string,   S:    50, U:   26, D:   24 |    1ms |          2ms |   350ms |       14ms |   778ms~|   291ms |     717ms~|         222ms |
| basic_string,   S:    50, U:   29, D:   21 |    1ms |          2ms |   332ms |       15ms |   781ms~|   669ms~|     717ms~|         237ms |
| basic_string,   S:   100, U:   46, D:   54 |    1ms |          2ms |   788ms~|       19ms |  1104ms~|   801ms~|     454ms |         782ms~|
| BigStr,         S:    50, U:   24, D:   26 |    1ms |          2ms |   970ms~|      610ms~|   983ms~|  1216ms~|     562ms~|        1258ms~|
| BigStr,         S:    50, U:   36, D:   14 |    1ms |          2ms |  1953ms~|      279ms |  1191ms~|  1363ms~|     760ms~|        1447ms~|
| BigStr,         S:   100, U:   45, D:   55 |    1ms |          2ms |  2332ms~|      328ms |  1620ms~|  1123ms~|    1533ms~|         852ms~|
| BigStr,         S:   500, U:  288, D:  212 |    1ms |          3ms | 38023ms~|     2556ms~| 12276ms~|  7148ms~|    8798ms~|        7388ms~|
| int,            S:    30, U:    9, D:   21 |    1ms |          2ms |    59ms |        6ms |    92ms |   471ms |     543ms~|          80ms |
| int,            S:    30, U:   30, D:    0 |    1ms |          2ms |    99ms |        7ms |   675ms~|   654ms~|     685ms~|         246ms |
| int,            S:   500, U:   99, D:  401 |    1ms |          2ms |   394ms |        8ms |  2883ms~|  1655ms~|    1911ms~|        1599ms~|
| int,            S:   500, U:  479, D:   21 |    1ms |          3ms |  1617ms~|        8ms | 10506ms~|  3953ms~|    7721ms~|        5541ms~|
