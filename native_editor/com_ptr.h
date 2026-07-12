#pragma once

namespace editor {

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* Get() const { return value_; }
    T** Put() {
        Reset();
        return &value_;
    }
    T* operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }

    void Reset() {
        if (value_) {
            value_->Release();
            value_ = nullptr;
        }
    }

private:
    T* value_ = nullptr;
};

}  // namespace editor
