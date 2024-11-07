#include "introspector.h"

// SIMPLE_DATA
struct Product {
    std::string name;
    uint32_t quantity = 0;
};

struct Fruits {
    Product apples;
    Product oranges;
    Product bananas;
    std::vector<Product> others;
};

// INTROSPECTOR_USE
int main() {
    Fruits my_shop { { "Apple", 200 }, { "Orange", 70 }, { "Banana", 80 } };

    TreeElementPrinter::visit(introspect(my_shop));

    return 0;
}