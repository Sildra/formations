
# Duplication de vecteur

La duplication de vecteur peut être une opération coûteuse. Prenons l'exemple d'un vecteur dont les doublons sont filtrés.


```cpp
// FILTER_VECTOR
```

Nous allons tenter de faire une version du filtre qui passe par un `set` ou un `unordered_set`. Les données sont copiées 2 fois et l'ordre n'est pas préservé dans ces tests.

```cpp
// FILTER_SET_TO_VECTOR
```

L'utilisation d'un `shared_ptr` est relativement simple. Dans notre cas, nous allons juste mettre le résultat du filtre dans le test et faire une copie di `shared_ptr`.


Le template de test se présente sous cette forme.

```cpp
// VECTOR_DUP_TEST
```

Pour facilement ajouter des données de tests, nous allons utiliser un générateur. Ce générateur utilise quelques principes de métaprogrammation pour notamment déduire le type de retour du générateur.

```cpp
// VECTOR_DUP_GEN
```

Et pour nos tests, nous allons les jouer avec ces données.

```cpp
// VECTOR_DUP_MAIN
```


```bash
# $CC intersect.cpp -O3 -o intersect.exe
# intersect.exe
```

```bash
> $CC --std=c++17 -O3 vector_dup.cpp -o vector_dup.exe
```
>! vector_dup.exe
