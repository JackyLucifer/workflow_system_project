/**
 * @file any.h
 * @brief C++11-compatible type-erased container (replacement for std::any)
 *
 * Design: Simple wrapper for arbitrary type storage
 * Compatible with C++11
 */

#ifndef WORKFLOW_SYSTEM_ANY_H
#define WORKFLOW_SYSTEM_ANY_H

#include <typeinfo>
#include <memory>
#include <stdexcept>

namespace WorkflowSystem {

/**
 * @brief C++11-compatible type-erased container
 *
 * Acts as a replacement for std::any (C++17+)
 */
class Any {
public:
    // Default constructor
    Any() : content_(nullptr) {}

    // Copy constructor
    Any(const Any& other) : content_(other.content_ ? other.content_->clone() : nullptr) {}

    // Move constructor
    Any(Any&& other) noexcept : content_(other.content_) {
        other.content_ = nullptr;
    }

    // Destructor
    ~Any() {
        delete content_;
    }

    // Template constructor
    template<typename T>
    Any(const T& value) : content_(new Holder<T>(value)) {}

    // Copy assignment
    Any& operator=(const Any& other) {
        if (this != &other) {
            Any(other).swap(*this);
        }
        return *this;
    }

    // Move assignment
    Any& operator=(Any&& other) noexcept {
        if (this != &other) {
            delete content_;
            content_ = other.content_;
            other.content_ = nullptr;
        }
        return *this;
    }

    // Template assignment
    template<typename T>
    Any& operator=(const T& value) {
        Any(value).swap(*this);
        return *this;
    }

    // Check if empty
    bool has_value() const noexcept {
        return content_ != nullptr;
    }

    // Alias for has_value (for convenience)
    bool isEmpty() const noexcept {
        return content_ == nullptr;
    }

    // Get type info
    const std::type_info& type() const noexcept {
        return content_ ? content_->type() : typeid(void);
    }

    // Reset
    void reset() noexcept {
        Any().swap(*this);
    }

    // Swap
    void swap(Any& other) noexcept {
        std::swap(content_, other.content_);
    }

    // Cast to type T
    template<typename T>
    T cast() const {
        if (!content_) {
            throw std::runtime_error("Bad any cast: empty");
        }
        if (typeid(T) != content_->type()) {
            throw std::runtime_error("Bad any cast: wrong type");
        }
        return static_cast<Holder<T>*>(content_)->value;
    }

private:
    // Base holder interface
    struct Placeholder {
        virtual ~Placeholder() = default;
        virtual const std::type_info& type() const noexcept = 0;
        virtual Placeholder* clone() const = 0;
    };

    // Typed holder
    template<typename T>
    struct Holder : public Placeholder {
        Holder(const T& value) : value(value) {}

        const std::type_info& type() const noexcept override {
            return typeid(T);
        }

        Placeholder* clone() const override {
            return new Holder(value);
        }

        T value;
    };

    Placeholder* content_;
};

// Convenience cast function (similar to std::any_cast)
template<typename T>
T any_cast(const Any& operand) {
    return operand.cast<T>();
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_ANY_H
