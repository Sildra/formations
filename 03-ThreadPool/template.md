# ThreadPool
Le Thread est un mécanisme d'exécution de tâches en arrière-plan. Il partage ses ressources avec un processus et peut disposer de son espace mémoire dédié (avec des ressources déclarées `thread_local`). La création d'un Thread est une opération réputée comme coûteuse.

En `C++`, deux structures sont disponibles dans le standard : le `std::thread` et le `std::jthread` depuis `C++20`.
À la différence des `std::jthread`, les `std::thread` doivent être joints ou détachés avant leur destruction.

La `ThreadPool` est un mécanisme de réutilisation et/ou de limitation de ressources.

# Structure

Pour gérer notre `ThreadPool`, nous avons besoin de :

* Stocker des `std::thread`
* Stocker des `Task`
* Notifier nos `std::thread` de nouvelles `Task` avec :
  * Une `std::condition_variable`
  * Un `std::mutex`
* Notifier la création de nos `std::thread`

```cpp
// DATA
```


# Construction

Pour la construction de la `ThreadPool`, nous nous contentons d'ajouter des threads dans notre collection. Nous attendons aussi que les threads soient démarrés et prêts à accepter de nouvelles tâches.

```cpp
// CTOR
```

# Thread exécution

La fonction d'exécution du thread est découpée en deux parties :
* La notification du constructeur
* La boucle de dépilement des tâches

La grande majorité de l'exécution se déroule avec le mutex acquis ce qui permet la bonne synchronisation des notifications. Ainsi le mutex est libéré durant 2 opérations :
* Pendant l'attente d'une notification de réveil de la part de la `ThreadPool`
* Pendant l'exécution d'une tâche, le mutex est acquis avant le dépilement d'une nouvelle tâche

L'implémentation actuelle permet la levée d'exception mais interromps le thread.

```cpp
// EXECUTE-THREAD
```

# Destruction

La destruction de la `ThreadPool` doit garantir la bonne libération des ressources. Pour notifier les threads, nous poussons une tâche de levée d'exception par thread actif puis nous attendons la fin de tous les threads.

```cpp
// DTOR
```

# Ordonnancement

```cpp
// SCHEDULE
```

# Tests

```cpp
// UTILS
```

Dans nos tests, nous allons tester avec une ThreadPool de 5 threads.

```cpp
// TEST-CREATION
```

Nous commençons avec de simples tâches d'affichage.

```cpp
// TEST-DISPLAY
```

Pour tester la protection de notre queue, nous allons utiliser un générateur de tâches. Ce générateur va se répliquer avec un compteur en générant ce compteur - 1 tâches. Au total, $1 + \sum_{i=n-1}^{1} \prod_{j=n-1}^i j$ tâches sont générées.


```cpp
// GENERATOR
```

Le thread principal va aussi exécuter une partie des tâches (et permettre d'attendre la fin de l'exécution du générateur).

```cpp
// TEST-GENERATOR
```

```bash
> $CC -std=c++14 main.cpp -pthread -O3 -o threads.exe
> threads.exe
```

# Pour aller plus loin

Cette implémentation minimaliste montre l'utilisation des `std::condition_variable` pour la notification inter-thread et les `std::mutex` pour protéger nos ressources. Des ajouts restent néanmoins possible :

* Ajouter un compteur de threads actifs,
* Ajouter des opérations d'ajout ou de relance de threads
* Optimiser l'exécution pendant la destruction afin de laisser les threads actifs le plus longtemps possible en cas de charge importante
* Permettre de bloquer l'ajout de tâches pendant la destruction
* Ajouter des mesures dans la ThreadPool (temps d'exécution, compteurs, ...)