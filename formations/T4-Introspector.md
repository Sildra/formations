[Sommaire](../README.md)


# Tree

## Structure

```cpp
struct TreeElement {
    std::string type;
    std::string field;
    std::string value;
    size_t size = 0;
    size_t offset = 0;
    std::vector<std::unique_ptr<TreeElement>> members;
};
```

## Visitor

```cpp
struct TreeElementVisitor {
    static void visit(const TreeElement& element, TreeElementVisitor& visitor) {
        visitor.visit_(element);
    }
    virtual void visit_(const TreeElement& node) = 0;
};

struct TreeElementPrinter : public TreeElementVisitor {
    size_t current_level = 0;

    static void visit(const TreeElement& root) {
        TreeElementPrinter printer;
        TreeElementVisitor::visit(root, printer);
    }

    void visit_(const TreeElement& node) override {
        std::cout << std::string(current_level * 2, ' ') << node.type
            << " " << node.field
            << " = " << node.value
            << " : " << node.size << "\n";
        ++current_level;
        for (const auto& element : node.members)
            TreeElementVisitor::visit(*element, *this);
        --current_level;
    }
};
```

## Tree helper

```cpp
template<typename T>
void fill_element(TreeElement& element, const T& value) {
    element.type = get_class_name<T>();
    element.value = std::format("0x{:x}", (size_t)&value);
    element.size = sizeof(value);
}

#define PRETTY_FILL_ELEMENT(CLASS, CLASS_NAME, TO_STRING)      \
template<>                                                      \
void fill_element(TreeElement& element, const CLASS& value) {   \
    element.type = CLASS_NAME;                                  \
    element.value = TO_STRING;                                  \
    element.size = sizeof(value);                               \
}

PRETTY_FILL_ELEMENT(int8_t,   "int8_t",   std::to_string(value));
PRETTY_FILL_ELEMENT(int16_t,  "int16_t",  std::to_string(value));
PRETTY_FILL_ELEMENT(int32_t,  "int32_t",  std::to_string(value));
PRETTY_FILL_ELEMENT(int64_t,  "int64_t",  std::to_string(value));
PRETTY_FILL_ELEMENT(uint8_t,  "uint8_t",  std::to_string(value));
PRETTY_FILL_ELEMENT(uint16_t, "uint16_t", std::to_string(value));
PRETTY_FILL_ELEMENT(uint32_t, "uint32_t", std::to_string(value));
PRETTY_FILL_ELEMENT(uint64_t, "uint64_t", std::to_string(value));
PRETTY_FILL_ELEMENT(std::string, "std::string", value);
#undef PRETTY_FILL_ELEMENT
```

# Introspection

## Scalar

```cpp
template<typename T, typename Enable = void>
struct introspection {
    template<size_t I>
    static void introspect_member(const T& value, TreeElement& node) { /* ERROR */ }
    static void introspect(const T& value, TreeElement& node) {
        fill_element(node, value);
    }
};
```

```cpp
template<typename T, size_t... I>
static void unfold_impl(const T& value, TreeElement& node, std::index_sequence<I...>) {
    (introspection<T>::template introspect_member<I>(value, node), ...);
}
template<typename T>
void unfold_type(const T& value, TreeElement& node) {
    unfold_impl(value, node, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
}
```

```cpp
template<typename T>
struct introspection<T , typename std::enable_if_t<std::is_aggregate_v<T>>> {
    template<size_t I>
    static void introspect_member(const T& value, TreeElement& node) {
        node.members.push_back(std::make_unique<TreeElement>());
        auto& member = *node.members.back();
        member.field = std::string(boost::pfr::get_name<I, T>());
        introspection<std::decay_t<decltype(boost::pfr::get<I>(value))>>
            ::introspect(boost::pfr::get<I>(value), member);
    }
    static void introspect(const T& value, TreeElement& node) {
        fill_element(node, value);
        unfold_type(value, node);
    }
};
```

```cpp
template<typename T>
TreeElement introspect(const T& value) {
    TreeElement result { .field = "root" };
    introspection<T>::introspect(value, result);
    return result;
}
```

# Main


```cpp
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
```

```cpp
int main() {
    Fruits my_shop { { "Apple", 200 }, { "Orange", 70 }, { "Banana", 80 } };

    TreeElementPrinter::visit(introspect(my_shop));

    return 0;
}
```



```bash
> $CC --std=c++20 -Ipfr/include main.cpp -o introspector.exe
```

```text
Fruits root = 0x75308ffc50 : 144
  Product apples = 0x75308ffc50 : 40
    std::string name = Apple : 32
    uint32_t quantity = 200 : 4
  Product oranges = 0x75308ffc78 : 40
    std::string name = Orange : 32
    uint32_t quantity = 70 : 4
  Product bananas = 0x75308ffca0 : 40
    std::string name = Banana : 32
    uint32_t quantity = 80 : 4
  vector others = 0x75308ffcc8 : 24
```