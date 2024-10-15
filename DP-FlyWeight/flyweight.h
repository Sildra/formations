#include <unordered_set>
#include <mutex>

namespace flyweight {
template<typename, typename> class FlyWeight;

template<typename T>
class FlyWeightElement final {
public:
    FlyWeightElement(const FlyWeightElement<T>& other) : value(other.value) {}
    operator const T&() const { return value; }
protected:
    template<typename, typename> friend class FlyWeight;
    FlyWeightElement(const T& value) : value(value) {}
private:
    const T& value;
};

template<typename T, typename H = std::hash<T>>
class FlyWeight final {
public:
    template<typename U>
    FlyWeightElement<T> get(U value) {
        std::lock_guard<decltype(mtx)> lk { mtx };
        return *values.insert(std::forward<T>(value)).first;
    }
    size_t size() {
        return values.size();
    }
private:
    std::unordered_set<T, H> values;
    std::mutex mtx;
};

} /* !namespace flyweight */