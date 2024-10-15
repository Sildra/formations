# La mémoire

## Introduction

* Présentation de la gestion de la mémoire : Importance, impact sur les performances, stabilité des applications


## Les types d'allocations

En C++, il existe 4 façons de gérer des objets (et par extension gérer la mémoire):

* Statique: le programme créé l'objet au lancement du programme,
  il est <span style="color:green">**automatiquement**</span> détruit à la fin du programme;
* Threadée: le programme créé l'objet au lancement du thread (ou fil d'exécution),
  il est <span style="color:green">**automatiquement**</span> détruit à la fin du thread;
* Automatique: le programme créé l'objet quand il en a besoin dans un contexte d'exécution (fonction, condition, boucle, ...),
  il est <span style="color:green">**automatiquement**</span> détruit à la fin du contexte;
* Dynamique: le **développeur** gère lui-même la création de l'objet (mot clé `new`),
  il a la <span style="color:red">**responsabilité**</span> de la destruction de l'objet en question (mot clé `delete`).


## Les zones d'allocation

Ces 4 façons d'allocation sont réparties en 2 zones mémoires : la pile et le tas:


* Le tas : cette zone est dédiée à la gestion de la mémoire dynamique de l'application. Toute instanciation dynamique va se retrouver dans cet espace (e.g. `new`, ajouter un objet dans une collection, instancier l'objet d'un smart-pointer, ...).

* La pile : cette zone mémoire est extrêmement rapide puisque en général elle est directement dans le cache du CPU. Dans des applications modernes, elle est allouée automatiquement par le compilateur et contiens l'espace nécessaire à l'exécution d'une fonction.
En plus de libérer la mémoire automatiquement, le compilateur va aussi ajouter des fonctions de destruction des objets C++. Cette libération s'effectue même en cas de levée d'exception et permet une simplification très importante des cas d'exception pour garantir un état correct de la pile. C'est sur ce mécanisme que l'idiome de programmation RAII a été inventé.

## RAII et le garbage collector


Contrairement à d'autres langages plus récents, il n'est pas nécessaire de prendre des mesures spécifiques pour ces cas :
* `with` en `Python`
* la fonction `dispose()` en `Java` et `C#`
* la construction `finally` en `Java`

Avec cette gestion, le garbage collector pensé en C++11 mais jamais totalement implémenté en est devenu redondant et a été supprimé du standard dans la version `C++23`.

Ces concepts de gestion des objets ont été décrits par l'inventeur du C++ (Bjarne Stroustrup) au milieu des
années 80 ([voir RAII](https://fr.wikipedia.org/wiki/Resource_acquisition_is_initialization)).

Aujourd'hui le `RAII` est considéré comme un standard industriel dans le développement en `C++`.

Prenons l'exemple suivant :

```c++
#include <string>

void thrower() { throw 0; }

int main() {
    std::string str1;
    std::string str2;
    thrower();
    return 0;
}
```

Pour simuler ce que le compilateur ferait nous allons effectuer les opérations suivantes :
* Allouer de la mémoire sur la pile avec une `arena`;
* Utiliser le placement-new sur l'espace réservé de l'`arena` pour construire nos `std::string`;
* implémenter un mécanisme de `try`/`catch`/`cleanup`/`rethrow` pour simuler le nettoyage de la pile.

```c++
#include <string>

void thrower() { throw 0; }

int main() {
    char arena[sizeof(std::string) * 2]; // Réservation de la mémoire de str1 et str2 sur la pile pour la fonction main
    std::string& str1 = reinterpret_cast<std::string&>(arena[0]);  // Association de la mémoire de arena à l'offset 0 pour str1
    std::string& str2 = reinterpret_cast<std::string&>(arena[sizeof(std::string)]); // Même principe pour str2
    try {
        new (&str1) std::string();      // Appel de la fonction de construction de std::string à l'emplacement de str1
        try {
            new (&str2) std::string();  // Appel de la fonction de construction de std::string à l'emplacement de str2
            thrower();              // On force une exception
        } catch (...) {
            str2.~basic_string();   // Cleanup de str2 en cas d'exception
            throw;                  // Levée de l'exception précédente
        }
    } catch (...) {
        str1.~basic_string();   // Cleanup de str1 en cas d'exception
        throw;                  // Levée de l'exception précédente
    }
    str2.~basic_string();   // Cleanup de str2 en cas de comportement normal
    str1.~basic_string();   // Cleanup de str1 en cas de comportement normal
    return 0;
}
```

Sous un explorateur d'assembleur tel que Godbolt, le premier programme ressemble à ceci (gcc14 -O0) :

```asm
thrower():
  push rbp
  mov rbp, rsp
  mov edi, 4
  call __cxa_allocate_exception
  mov DWORD PTR [rax], 0
  mov edx, 0
  mov esi, OFFSET FLAT:typeinfo for int
  mov rdi, rax
  call __cxa_throw
main:
  push rbp
  mov rbp, rsp
  push rbx
  sub rsp, 72
  lea rax, [rbp-48]
  mov rdi, rax
  call std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string() [complete object constructor]
  lea rax, [rbp-80]
  mov rdi, rax
  call std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string() [complete object constructor]
  call thrower()
  // Exécuté dans une operation normale, l'instruction jmp correspond au return
  mov ebx, 0
  lea rax, [rbp-80]
  mov rdi, rax
  call std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >::~basic_string() [complete object destructor]
  lea rax, [rbp-48]
  mov rdi, rax
  call std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >::~basic_string() [complete object destructor]
  mov eax, ebx
  jmp .L6
  // Exécuté en cas d'exception, le `call _Unwind_Resume` permet de continuer le nettoyage de la pile
  mov rbx, rax
  lea rax, [rbp-80]
  mov rdi, rax
  call std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >::~basic_string() [complete object destructor]
  lea rax, [rbp-48]
  mov rdi, rax
  call std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >::~basic_string() [complete object destructor]
  mov rax, rbx
  mov rdi, rax
  call _Unwind_Resume
.L6:
  mov rbx, QWORD PTR [rbp-8]
  leave
  ret
```

Entre l'assembleur généré et notre second code, nous retrouvons les appels aux 2 constructeurs `basic_string()` et 4 destructeurs `~basic_string()`. 
Bien sûr, le code produit par le compilateur permet d'optimiser la gestion du nettoyage de la pile


## Les objets

En C++, il existe plusieurs concepts pour définir un objet :

* Un objet: Un objet dont la création et la destruction est <span style="color:green">**automatique**</span>.
  ```c++
    struct Base {};
    int main() {
        Base base;  // Objet créé automatiquement, détruit automatiquement
        return 0;
    }
  ```
* Un pointeur (`*`): Une adresse mémoire vers un objet **existant ou non**. Doit être détruit <span style="color:red">**manuellement**</span> si l'objet a été créé manuellement.
  ```c++
    struct Base {};
    int main() {
        Base base;                  // Objet créé automatiquement, détruit automatiquement
        Base* noBase = nullptr;     // Pointeur sans objet associé, pas de destruction necéssaire
        Base* ptrToBase = &base;    // Pointeur sur la base
        Base* newBase = new Base(); // Objet créé dynamiquement, à libérer
        delete newBase;             // Détruit manuellement
        return 0;
    }
  ```
* Une référence (`&`): Une adresse mémoire vers un objet **existant**. L'objet référencé doit survivre la référence pour garantir un comportement standard (appel de fonction par exemple). La référence se comporte comme un pointeur qui est automatiquement déréférencé.
  ```c++
    struct Base {};
    int main() {
        Base base;                  // Objet créé automatiquement, détruit automatiquement
        Base& refToBase = base;     // Référence sur l'objet
        return 0;
    }
  ```

De ces 3 concepts on peut définir 2 classes d'objets en particuliers.

* Les collections : la collection peut être définie sur la pile ou le tas mais son contenu est en général dynamique et alloué sur le tas.
  ```c++
    #include <vector>
    struct Base {};
    int main() {
        std::vector<Base> bases; // Alloué sur la pile
        bases.push_back(Base()); // Alloué sur le tas
        return 0;
    }
  ```
* Les smart-pointers : ce sont des wrappers sur des pointeurs permettant de bénéficier des avantages du RAII. Leur gestion est <span style="color:green">**automatique**</span>.
  * Le plus simple est l'`unique_ptr`. Il est le remplacement à privilégier de la gestion de mémoire dynamique. Il n'est pas copiable donc un seul objet peut le posséder. En termes de performance, son coût est similaire à l'utilisation de `new`/`delete`.
  ```c++
    #include <memory>
    struct Base {};
    void use(const Base&);    // Forward declaration d'une fonction
    int main() {
        std::unique_ptr<Base> base;         // Déclaration d'un objet sur la pile
        base = std::make_unique<Base>();    // Association d'un nouvel objet alloué sur le tas
        if (base)                           // Test de présence d'un objet
            use(*base.get());               // Utilisation de l'objet
        return 0;                           // Destruction automatique de base en sortie de scope
    }
  ```
    * Le second est le `shared_ptr`. Il est à privilégier en cas de responsabilité partagée ou dans certains cas de multithreading. Son coût de création/copie/destruction est légèrement plus élevé que l'`unique_ptr` puisqu'il embarque un compteur de références qui est mis à jour de manière atomique entre les différents threads.
  
  ```c++
  ```

## L'héritage


Une bonne gestion de mémoire passe aussi par une bonne gestion de l'héritage.

En C++, l'un des points importants pour que la symétrie fonctionne correctement est de déclarer des méthodes virtuelles.
Exemple pour une fonction d'affichage:

* Sans méthode virtuelle: j'appelle l'affichage d'un objet, j'exécute l'affichage de l'objet.
* Avec méthode virtuelle: j'appelle l'affichage d'un objet, si c'est un hôpital, j'exécute l'affichage de l'hôpital, sinon j'exécute l'affichage de l'objet.

Pour la destruction de l'objet, c'est pareil. Sans destructeur virtuel, on ne détruit pas les objets issus de l'héritage.
