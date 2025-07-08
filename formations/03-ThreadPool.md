[Sommaire](../README.md)

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
std::mutex mutex;                           // Mutex for incomming jobs
std::condition_variable waiter;             // Notify incomming jobs
std::condition_variable pool;               // Notify thread creation
std::vector<std::thread> executors;         // Executor threads
std::deque<UniqueTask> tasks;               // Task collection
```


# Construction

Pour la construction de la `ThreadPool`, nous nous contentons d'ajouter des threads dans notre collection. Nous attendons aussi que les threads soient démarrés et prêts à accepter de nouvelles tâches.

```cpp
ThreadPool(int count)
{
    std::unique_lock<std::mutex> notif { mutex };
    for (; count > 0; --count) {
        executors.push_back(std::thread(&ThreadPool::execute_thread, this));
        pool.wait(notif);
    }
}
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
void execute_thread()
{
    std::unique_lock<std::mutex> notif { mutex };
    pool.notify_one();

    try {
        while (true) {
            waiter.wait(notif);
            while (auto task = get_task()) {
                notif.unlock();
                (*task)();
                notif.lock();
            }
        }
    } catch (const std::exception&) {}
}
```

# Destruction

La destruction de la `ThreadPool` doit garantir la bonne libération des ressources. Pour notifier les threads, nous poussons une tâche de levée d'exception par thread actif puis nous attendons la fin de tous les threads.

```cpp
~ThreadPool()
{
    std::vector<UniqueTask> tasks;
    tasks.reserve(executors.size());
    for (size_t i = 0; i < executors.size(); ++i)
        tasks.push_back(std::make_unique<Task>([]() { throw StopException(); } ));
    schedule(std::move(tasks));
    for (auto& executor : executors) {
        executor.join();
    }
    while (auto f = get_task()) {
        (*f)();
    }
}
```

# Ordonnancement

TODO

```cpp
template<typename C>
void schedule(C&& coll)
{
    std::lock_guard<std::mutex> lock { mutex };
    for (auto&& task : coll)
        tasks.push_back(std::move(task));
    waiter.notify_all();
}

void schedule(UniqueTask&& task)
{
    std::lock_guard<std::mutex> lock { mutex };
    tasks.push_back(std::move(task));
    waiter.notify_one();
}
```

# Tests

TODO

```cpp
static int global_id = 0;
const std::string get_id()
{
    thread_local const std::string id = std::string("Thread ").append(std::to_string(global_id++));
    return id;
}

void display(const std::string& val)
{
    std::string value = get_id();
    value.append(" ").append(val).append("\n");
    std::cout << value;
}

void showTime(const std::string& info, std::chrono::high_resolution_clock::time_point& start)
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << info << " executed in " << duration << "ms\n";
}
```

Dans nos tests, nous allons tester avec une ThreadPool de 5 threads.

```cpp
auto now = std::chrono::high_resolution_clock::now();
thread_pool::ThreadPool tp(5);
showTime("ThreadPool creation", now);
```

Nous commençons avec de simples tâches d'affichage.

```cpp
using namespace std::chrono_literals;
auto f1 = thread_pool::Task([]() { display("Display()"); std::this_thread::sleep_for(1ms); });
auto f2 = thread_pool::Task([]() { display("Display2()"); std::this_thread::sleep_for(1ms); });
std::vector<thread_pool::UniqueTask> displayTasks;
for (auto& task : { f1, f1, f1, f2, f1, f2, f1, f1 })
    displayTasks.push_back(std::make_unique<thread_pool::Task>(task));
tp.schedule(std::move(displayTasks));
std::this_thread::sleep_for(15ms);
```

Pour tester la protection de notre queue, nous allons utiliser un générateur de tâches. Ce générateur va se répliquer avec un compteur en générant ce compteur - 1 tâches. Au total, $1 + \sum_{i=n-1}^{1} \prod_{j=n-1}^i j$ tâches sont générées.


```cpp
struct TaskGenerator
{
    thread_pool::ThreadPool& thread_pool;
    int generator_count;
    TaskGenerator(thread_pool::ThreadPool& pool, int generator_count)
        : thread_pool(pool), generator_count(generator_count) {}

    void execute()
    {
        if (generator_count < 1)
            return;
        thread_pool::ThreadPool* tp = &thread_pool;
        int gc = generator_count - 1;
        std::vector<thread_pool::UniqueTask> generator;
        generator.reserve(gc);
        for (int i = 0; i < gc; ++i)
            generator.push_back(std::make_unique<thread_pool::Task>([=](){ TaskGenerator(*tp, gc).execute(); }));
        thread_pool.schedule(std::move(generator));
    }
};
```

Le thread principal va aussi exécuter une partie des tâches (et permettre d'attendre la fin de l'exécution du générateur).

```cpp
constexpr int operation_count = 10;
now = std::chrono::high_resolution_clock::now();
tp.schedule(std::make_unique<thread_pool::Task>([&]() { TaskGenerator(tp, operation_count).execute(); }));
while (tp.execute())
    ;
// Compute the number of operations
int acc = 1;
int tot = 1;
for (int  i = operation_count - 1; i > 0; --i) {
    acc *= i;
    tot += acc;
}
showTime(std::to_string(tot) + " tasks", now);
```

La compilation nécessite le flag de link `-pthread`.

```bash
> $CC -std=c++14 main.cpp -pthread -O3 -o threads.exe
> threads.exe
ThreadPool
ThreadPool creation executed in 2ms
Thread 0 Display()
Thread 1 Display()
Thread 2 Display()
Thread 3 Display2()
Thread 4 Display()
Thread 3 Display2()
Thread 1 Display()
Thread 2 Display()
986410 tasks executed in 550ms
End ThreadPool
```

# Pour aller plus loin

Cette implémentation minimaliste montre l'utilisation des `std::condition_variable` pour la notification inter-thread et les `std::mutex` pour protéger nos ressources. Des ajouts restent néanmoins possible :

* Ajouter un compteur de threads actifs,
* Ajouter des opérations d'ajout ou de relance de threads
* Optimiser l'exécution pendant la destruction afin de laisser les threads actifs le plus longtemps possible en cas de charge importante
* Permettre de bloquer l'ajout de tâches pendant la destruction
* Ajouter des mesures dans la ThreadPool (temps d'exécution, compteurs, ...)