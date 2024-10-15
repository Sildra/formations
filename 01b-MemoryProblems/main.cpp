#include <iostream>

static bool isStackPointer(void* ptr);

#ifdef _WIN64
# pragma message("Using Windows")
# include <Windows.h>

static bool isStackPointer(void* ptr) {
    NT_TIB* threadBlock = (NT_TIB*)NtCurrentTeb();

    return ptr >= threadBlock->StackLimit && ptr <= threadBlock->StackBase;
}

#else
# pragma message("Using Unix")
# include <unistd.h>
# include <sys/resource.h>

# if 0
static bool isStackPointer(void* ptr) {
    char stackAddress;
    void* address = (void*)&stackAddress;
    rlimit limit;
    getrlimit(RLIMIT_STACK, &limit);
    intptr_t curr = reinterpret_cast<intptr_t>(ptr);
    intptr_t start = reinterpret_cast<intptr_t>(address) & ~(limit.rlim_cur - 1);
    intptr_t end = start + limit.rlim_cur;
    return (curr >= start && curr <= end);
}
# else
#  include <fstream>
#  include <array>
#  include <cstring>
static bool isStackPointer(void* ptr) {
    std::ifstream file {"/proc/self/maps"};
    intptr_t curr = reinterpret_cast<intptr_t>(ptr);
    char line[512];
    while (file.getline(&line[0], sizeof(line))) {
        char* cursor = line;
        unsigned long long start = std::strtoull(cursor, &cursor, 16);
        ++cursor;
        unsigned long long end = std::strtoull(cursor, &cursor, 16);
        if (curr <= start || curr >= end)
            continue;
        cursor = strchr(cursor, '[');
        return (cursor && strncmp(cursor, "[stack", 6) == 0);
    }
    return false;  
}
# endif
#endif

template<typename T>
class CheckedPointer {
public:
    CheckedPointer(T* p = nullptr) : ptr(p) {
        std::cout << "New checked pointer is stack pointer: " << std::boolalpha << isStackPointer(ptr) << "\n";
    }
    T&  operator*() const { return *ptr; }
    T*  operator->() const { return ptr; }
    operator T*() const { return ptr; }
private:
    T* ptr;
};

int main() {
    char stackVal;
    int* heapVal = (int*)malloc(sizeof(int));
    CheckedPointer<int> checkedHeapVal(heapVal);
    checkedHeapVal = heapVal;
    free(checkedHeapVal);
    checkedHeapVal = new int(4);
    delete checkedHeapVal;

    std::cout << "char stackVal; is stack pointer: " << std::boolalpha << isStackPointer(&stackVal) << "\n";
    std::cout << "int* heapVal; is stack pointer: " << std::boolalpha << isStackPointer(heapVal) << "\n";
    return 0;
}