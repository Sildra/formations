[Sommaire](../README.md)

[Sommaire](../README.md)

# Objectif de l'exercice

Cet exercice a pour objectif d'évaluer les connaissances C++ suivantes des candidats:

* Gestion mémoire
* Conception logicielle et POO
* Exactitude du langage

# Code

## Exercice

Ceci est le code soumis au candidat pour l'évaluation.

```c++
#include <vector>
#include <iostream>
#include <string>

struct Building {
    Building(int id) : Id(id) {}
    virtual std::string& display() {
        return std::string("Building ").append(std::to_string(Id));
    }

    int Id;
};
struct Hospital : public Building {
    Hospital(const char* name) : Building(0), Name(name) {}
    const char* Name;
};

struct City : std::vector<Building*> {
    City(const char* name) : Name(name) {}
    void addBuilding(int id) { push_back(new Building(id)); }
    void addHospital(const char* name) { push_back(new Hospital(name)); }
    const char* Name;
};

static void displayCity(City city) {
    std::cout << "In the city of " << city.Name << " we have:\n";
    for (std::vector<Building*>::iterator current = city.begin(); current != city.end(); current++) {
        if (auto hospital = dynamic_cast<Hospital*>(*current))
            std::cout << "    Hospital " << hospital->Name << "\n";
        else
            std::cout << "    Building " << (*current)->Id << "\n";
    }
}

int main() {
    std::vector<Building*>* newYork = new City("New York");
    ((City*)newYork)->addBuilding(42);
    ((City*)newYork)->addBuilding(20);
    {
        char hospitalName[70];
        sprintf(hospitalName, "%s's %s", static_cast<City*>(newYork)->Name, "Presbyterian/Columbian Univerity Medical Center");
        ((City*)newYork)->addHospital(hospitalName);
        displayCity(*(static_cast<City*>(newYork)));
    }
    return 0;
}
```

Le résultat du programme est décrit ci-dessous, avec une analyse de la mémoire en fin du programme.

```c++
"In the city of New York we have:
    Building 42
    Building 20
    Hospital New York's Presbyterian/Columbian Univerity Medical Center"

// In use at exit: 120 bytes in 5 blocks
```

### Explication de l'exercice

En C++, il existe 4  façons de gérer des objets (et par extension la mémoire):

* Statique: le programme créé l'objet au lancement du programme,
  il est <span style="color:green">**automatiquement**</span> détruit à la fin du programme;
* Threadée: le programme créé l'objet au lancement du thread (ou fil d'exécution),
  il est <span style="color:green">**automatiquement**</span> détruit à la fin du thread;
* Automatique: le programme créé l'objet quand il en a besoin dans un contexte d'exécution (fonction, condition, boucle, ...),
  il est <span style="color:green">**automatiquement**</span> détruit à la fin du contexte;
* Dynamique: le **développeur** gère lui-même la création de l'objet (mot clé `new`),
  il a la <span style="color:red">**responsabilité**</span> de la destruction de l'objet en question (mot clé `delete`).


Cette symétrie création/destruction (ou new/delete) est LE concept fondamental en C++ qui distingue
la capacité d'un développeur a produire du code de qualité et ce qui rends le langage difficile à appréhender.
Ces concepts de gestion des objets ont été décrits par l'inventeur du C++ (Bjarne Stroustrup) à la fin des
années 80 ([cf. RAII](https://fr.wikipedia.org/wiki/Resource_acquisition_is_initialization)).

L'objectif de l'exercice est de soumettre le candidat à une gestion de mémoire défaillante ainsi qu'une conception logicielle bancale.

## Solution

```c++
#include <vector>
#include <iostream>
#include <string>
#include <memory>

struct Building {
    Building(int id) : Id(id) {}
    virtual ~Building() = default;
    virtual std::string display() const {
        return std::string("Building ").append(std::to_string(Id));
    }

    int Id;
};
struct Hospital : public Building {
    Hospital(std::string name) : Building(0), Name(name) {}
    virtual ~Hospital() = default;
    virtual std::string display() const {
        return std::string("Hospital ").append(Name);
    } 
    std::string Name;
};

struct City {
    City(std::string name) : Name(name) {}

    std::string Name;
    std::vector<std::unique_ptr<Building>> buildings;

    void addBuilding(int id) {
        buildings.push_back(std::make_unique<Building>(id));
    }
    void addHospital(const char* name) {
        buildings.push_back(std::make_unique<Hospital>(name));
    }

    void display() {
        std::cout << "In the city of " << city.Name << " we have:\n";
        for (const auto& building : buildings)
            std::cout << "    " << building.display() << "\n";
    }

    static void displayCity(const City& city) {
        city.display();
    }
};

int main() {
    auto newYork = City("New York");
    newYork.addBuilding(42);
    newYork.addBuilding(20);
    newYork.addHospital(
        std::string(newYork.Name).append("'s Presbyterian/Columbian Univerity Medical Center")
    );
    newYork.display();
    return 0;
}
```

Dans cette nouvelle version du code, la symétrie construction/destruction est entièrement gérée <span style="color:green">**automatiquement**</span>.
Le langage garantie la libération des ressources **quelque soit l'exécution du programme**.

```c++
"In the city of New York we have:
    Building 42
    Building 20
    Hospital New York's Presbyterian/Columbian Univerity Medical Center"

// In use at exit: 0 byte in 0 blocks
```

## Explication détaillée

### Partie 1: Les objets (Building et Hospital)

Le premier point concerne la conception de la relation entre l'hôpital et le bâtiment.

Un hôpital est un bâtiment. En programmation orientée objet, cette relation s'appelle l'héritage.
On dit que `Hospital` hérite de `Building`.

En C++, l'un des points importants pour que la symétrie fonctionne correctement est de déclarer des méthodes virtuelles.
Exemple pour une fonction d'affichage:

* Sans méthode virtuelle: j'appelle l'affichage d'un bâtiment, j'exécute l'affichage du bâtiment.
* Avec méthode virtuelle: j'appelle l'affichage d'un bâtiment, si c'est un hôpital, j'exécute l'affichage de l'hôpital, sinon j'exécute l'affichage du bâtiment.

Pour la destruction de l'objet, c'est pareil. Sans destructeur virtuel, on ne détruit pas les objets issus de l'héritage.

Petite subtilité du C++, il existe plusieurs concepts pour définir un objet:

* Un objet: Un objet dont la création et la destruction est <span style="color:green">**automatique**</span>.
  ```c++
  struct Hospital {};
  Hospital hospital; // Objet créé automatiquement, détruit automatiquement
  ```
* Une référence (`&`): Une adresse mémoire vers un objet **existant**. N'est jamais détruit.
  ```c++
  struct Hospital {};
  Hospital hospital; // Objet créé automatiquement, détruit automatiquement
  Hospital& referenceToHospital = hospital; // Référence sur l'hopital
  ```
* Un pointeur (`*`): Une adresse mémoire vers un objet **existant ou non**. Doit être détruit <span style="color:red">**manuellement**</span> si l'objet a été créé manuellement.
  ```c++
  struct Hospital {};
  Hospital hospital; // Objet créé automatiquement, détruit automatiquement
  Hospital* no_hospital = nullptr; // Pointeur sans objet associé, pas de destruction
  Hospital* pointerToHospital = &hospital; // Pointeur sur l'hopital
  Hospital* newHospital = new Hospital(); // Pointeur créé manuellement
  delete newHospital; // Détruit manuellement
  ```

```c++
// # Problème
struct Building {
    Building(int id) : Id(id) {}
    // Problème: ici, nous ne déclarons pas de destructeur virtuel (virtual ~Building) -> destruction des enfants incomplète
    virtual std::string& display() { // ERREUR: ici on renvoie une référence (&) vers un objet local. L'objet local est détruit à la fin de la fonction
        std::string toDisplay;
        toDisplay = "Building " + std::to_string(Id);
        return toDisplay;
    }

    int Id;
}
struct Hospital : public Building {
    Hospital(const char* name) : Building(0), Name(name) {}
    // On a pas de destructeur non plus ? Normal ça fait partie de l'exercice.
    // On a pas de fonction display() ? et la cohérence avec Building ?
    const char* Name; // Pointeur -> les pointeurs c'est le MAAAAL. On détruit ? on détruit pas ?
};

// # Solution
struct Building {
    Building(int id) : Id(id) {}
    virtual ~Building() = default; // Destructeur virtuel -> cool
    virtual std::string display() const { // On renvoie un objet et pas une référence sui un objet qui a été détruit -> cool
        return std::string("Building ").append(std::to_string(Id));
    }

    int Id;
};
struct Hospital : public Building {
    Hospital(std::string name) : Building(0), Name(name) {}
    virtual ~Hospital() = default;
    virtual std::string display() const { // 
        return std::string("Hospital ").append(Name);
    } 
    std::string Name; // Objet -> là c'est clean, on construit et on détruit proprement
};
```

### Partie 2: La collection (City)

```c++
// # Problème
struct City : std::vector<Building*> {  // Une vile ne se résume pas à une liste de batiments
    City(const char* name) : Name(name) {}
    const char* Name; // Comme pour l'hopital: On détruit ? on détruit pas ?
};

static void displayCity( // displayCity ne fait pas partie de City ? Problème de conception.
    City city // Ici on passe un objet. Un nouvel objet. Donc pleins de copies dans tous les sens
    ) {
    std::cout << "In the city of " << city.Name << " we have:\n";
    for (std::vector<Building*>::iterator current = city.begin(); current != city.end(); current++) { // Boucle trop complexe, l'intention est difficile à comprendre
        if (auto hospital = dynamic_cast<Hospital*>(*current)) // On cherche à savoir si notre objet est un Hospital ou pas ?
            std::cout << "    Hospital " << hospital->Name << "\n";
        else
            std::cout << "    Building " << (*current)->Id << "\n"; // 
    }
}
// # Solution
struct City {
    City(std::string name) : Name(name) {}

    std::string Name;
    std::vector<std::unique_ptr<Building>> buildings; // Une ville contient des batiments mais on peut ajouter des parcs après. Conception modulable.

    void display() {
        std::cout << "In the city of " << city.Name << " we have:\n";
        for (const auto& building : buildings) // boucle simple, on regarde chaque building
            std::cout << "    " << building.display() << "\n"; // affichage normalisé, on ne regarde pas si un batiment est un hopital
    }

    static void displayCity(const City& city) { // Autre façon d'écrire l'affichage. On passe une référence sur un objet qui existe déjà, donc pas de copie
        city.display();
    }
};
```

### Partie 3: La création de la ville et sa destruction

```c++
// # Problème
int main() {
    std::vector<Building*>* newYork = new City("New York"); // On a un pointeur mais pas de delete ? pourquoi pas un objet créé automatiquement ?
    newYork->push_back(new Building(42)); // Pas de delete
    newYork->push_back(new Building(20)); // Pas de delete
    {
        char hospitalName[70]; // Objet local associé à l'hopital. La construction du nom de l'hopital est trop complexe.
        sprintf(hospitalName, "%s's %s", static_cast<City*>(newYork)->Name, "Presbyterian/Columbian Univerity Medical Center");
        newYork->push_back(new Hospital(hospitalName)); // Problème complexe ici: on associe un objet local à l'hopital (hospitalName). 
                                                        // Cet objet est détruit à la fin du contexte (au prochain '}' )
        displayCity(*(static_cast<City*>(newYork))); // Trop complexe
    }

    // Ceci est une alternative moche aux problèmes de delete. Cette solution est trop complexe mais remplit à peu près la problématique
#if 0
    for (auto& building : newYork) // on parcours les buildings de la ville
        delete building; // on les détruits
    delete newYork; // et on détruit la ville au final
#endif // Fin de l'alternative moche

    return 0;
}
// # Solution
int main() {
    auto newYork = City("New York");  // Objet créé automatiquement donc détruit automatiquement
    newYork.addBuilding(42); // Dans l'ensemble des appels sont moins complexe
    newYork.addBuilding(20);
    newYork.addHospital(
        std::string(newYork.Name).append("'s Presbyterian/Columbian Univerity Medical Center")
    );
    newYork.display();
    return 0;
}
int main() {
    auto newYork = City("New York");
    newYork.buildings.push_back(std::make_unique<Building>(42)); // Pas de new, pas besoin de delete
    newYork.buildings.push_back(std::make_unique<Building>(20));
    std::string hospitalName = std::string(newYork.Name).append("'s Presbyterian/Columbian Univerity Medical Center");
    newYork.buildings.push_back(
        std::make_unique<Hospital>(hospitalName)); // OK: On copie l'objet (hospitalName),
                                                   // il sera détruit automatiquement par l'hopital
    newYork.display(); // Simple
    return 0;
}
```