#include "introspector.h"

struct person {
    std::string name;
    uint32_t birth_year = 1900;
};

struct family {
    person father;
    person mother;
    person son;
    std::vector<int> toto;
};

int main() {
    family my_family { { "Edgard", 1809 }, { "Edgarette", 1830 }, { "Edgaron", 1845 } };

    TreeElementPrinter::visit(introspect(my_family));

    return 0;
}