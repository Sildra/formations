#include <deque>
#include <mutex>

template<typename T>
struct ProtectedDeque
{
    std::deque<T> collection;
    std::mutex mutex;
    void push(T&& item)
    {
        std::lock_guard<std::mutex> lk { mutex };
        collection.push_back(std::move(item));
    }
    T pop()
    {
        std::lock_guard<std::mutex> lock { mutex };
        if (collection.empty())
            return T();
        ++counter;
        T item { std::move(collection.front()) };
        collection.pop_front();
        return item;
    }
};