#pragma once

#include <memory>
#include <mutex>

namespace rcu_atomic {
    template<typename T>
    class RCU {
    public:
        // Construct
        RCU() = default;
        RCU(T* ptr) : shared_data(ptr) {}
        // Copy

        RCU(const RCU& other) : shared_data(other.get_shared()) {}
        RCU& operator=(const RCU& other) { return *this = other.get_shared(); }
        RCU& operator=(const std::shared_ptr<const T>& other) {
            std::lock_guard update_lock { update_mtx };
            shared_data = other;
            return *this;
        }
        // Move
        RCU(RCU&& other) : shared_data(std::move(other.shared_data)) { }
        RCU(std::shared_ptr<const T>&& other) : shared_data(std::move(other)) { }
        RCU& operator=(RCU&& other) { return *this = std::move(other.shared_data); }
        RCU& operator=(std::shared_ptr<const T>&& other) {
            std::lock_guard<std::mutex> update_lock { update_mtx };
            shared_data = std::move(other);
            return *this;
        }

        // Access
        std::shared_ptr<const T> get_shared() const {
            return shared_data;
        }
        explicit operator std::shared_ptr<const T>() const { return get_shared(); }
        explicit operator bool() const noexcept { return (bool)shared_data; }
        // Update
        template<typename Updater>
        void update(Updater updater) {
            std::lock_guard<std::mutex> write_lock { update_mtx };
            if (!shared_data.load())
                return;
            auto new_data = updater(*shared_data.load());
            shared_data = std::move(new_data);
        }
        template<typename Updater>
        void inline_update(Updater updater) {
            std::lock_guard<std::mutex> write_lock { update_mtx };
            if (!shared_data)
                return;
            updater(const_cast<T&>(*shared_data));
        }
        std::mutex update_mtx;
        std::atomic<std::shared_ptr<const T>> shared_data;
    };
}