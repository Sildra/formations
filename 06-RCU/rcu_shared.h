#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>

namespace rcu_shared {
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
            std::scoped_lock update_lock { update_mtx };
            std::scoped_lock shared_lock { shared_mtx };
            shared_data = other;
            return *this;
        }
        // Move
        RCU(RCU&& other) : shared_data(std::move(other.shared_data)) { }
        RCU(std::shared_ptr<const T>&& other) : shared_data(std::move(other)) { }
        RCU& operator=(RCU&& other) { return *this = std::move(other.shared_data); }
        RCU& operator=(std::shared_ptr<const T>&& other) {
            std::scoped_lock update_lock { update_mtx };
            std::scoped_lock shared_lock { shared_mtx };
            shared_data = std::move(other);
            return *this;
        }

        // Access
        std::shared_ptr<const T> get_shared() const {
            std::shared_lock shared_lock { shared_mtx };
            return shared_data;
        }
        explicit operator std::shared_ptr<const T>() const { return get_shared(); }
        explicit operator bool() const noexcept { return (bool)shared_data; }
        // Update
        template<typename Updater>
        void update(Updater updater) {
            std::scoped_lock write_lock { update_mtx };
            if (!shared_data)
                return;
            auto new_data = updater(*shared_data);
            std::scoped_lock shared_lock { shared_mtx };
            shared_data = std::move(new_data);
        }
        template<typename Updater>
        void inline_update(Updater updater) {
            std::scoped_lock write_lock { update_mtx };
            if (!shared_data)
                return;
            updater(const_cast<T&>(*shared_data));
        }
        std::mutex update_mtx;
        mutable std::shared_mutex shared_mtx;
        std::shared_ptr<const T> shared_data;
    };
}