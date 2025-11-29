#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace RefactorPipeline {

class ServiceContainer {
public:
    template <typename TService>
    void registerSingleton(std::function<std::shared_ptr<TService>(ServiceContainer&)> factory) {
        const std::type_index key(typeid(TService));
        registrations_[key] = Registration{
            [factory](ServiceContainer& container) -> std::shared_ptr<void> {
                return factory(container);
            },
            nullptr
        };
    }

    template <typename TService>
    std::shared_ptr<TService> resolve() {
        const std::type_index key(typeid(TService));
        auto it = registrations_.find(key);
        if (it == registrations_.end()) {
            throw std::runtime_error("Service not registered");
        }

        if (!it->second.instance) {
            it->second.instance = it->second.factory(*this);
        }

        return std::static_pointer_cast<TService>(it->second.instance);
    }

private:
    struct Registration {
        std::function<std::shared_ptr<void>(ServiceContainer&)> factory;
        std::shared_ptr<void> instance;
    };

    std::unordered_map<std::type_index, Registration> registrations_;
};

} // namespace RefactorPipeline

