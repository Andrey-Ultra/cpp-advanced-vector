#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>


template<typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
          , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory &) = delete;

    RawMemory &operator=(const RawMemory &) = delete;

    RawMemory(RawMemory &&other) noexcept {
        Swap(other);
    }

    RawMemory &operator=(RawMemory &&rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    T *operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept {
        return const_cast<RawMemory &>(*this) + offset;
    }

    const T &operator[](size_t index) const noexcept {
        return const_cast<RawMemory &>(*this)[index];
    }

    T &operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept {
        return buffer_;
    }

    T *GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T *Allocate(size_t n) {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T *buf) noexcept {
        operator delete(buf);
    }

    T *buffer_ = nullptr;
    size_t capacity_ = 0;
};


template<typename T>
class Vector {
public:
    using iterator = T *;
    using const_iterator = const T *;

    Vector() noexcept = default;

    Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector &other) : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }

    Vector(Vector &&other) noexcept {
        Swap(other);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args &&... args) {
        assert(pos >= begin() && pos <= end());
        size_t index = pos - begin();
        if (size_ == data_.Capacity()) {
            return EmplaceWithRealloc(index, std::forward<Args>(args)...);
        }
        return EmplaceWithoutRealloc(index, std::forward<Args>(args)...);
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        size_t index = pos - begin();
        std::move(begin() + index + 1, end(), begin() + index);
        (end() - 1)->~T();
        size_--;
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T &value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T &&value) {
        return Emplace(pos, std::move(value));
    }


    Vector &operator=(const Vector &rhs) {
        if (data_.Capacity() >= rhs.Size()) {
            if (size_ >= rhs.Size()) {
                std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                DestroyN(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
            } else {
                std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                std::uninitialized_copy_n(rhs.data_.GetAddress() + size_,
                                          rhs.size_ - size_,
                                          data_.GetAddress() + size_);
            }
            size_ = rhs.size_;
        } else {
            Vector<T> new_vector(rhs);
            Swap(new_vector);
        }
        return *this;
    }

    Vector &operator=(Vector &&rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    void Swap(Vector &other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t capacity) {
        if (capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(capacity);


        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        data_.Swap(new_data);
        DestroyN(new_data.GetAddress(), size_);
    }

    void Resize(size_t new_size) {
        if (new_size > data_.Capacity()) {
            Reserve(new_size);
        }

        if (new_size < size_) {
            DestroyN(data_.GetAddress() + new_size, size_ - new_size);
        }
        if (new_size > size_) {
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T &operator[](size_t index) const noexcept {
        return const_cast<Vector &>(*this)[index];
    }

    T &operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void PopBack() noexcept {
        data_[size_ - 1].~T();
        --size_;
    }

    void PushBack(const T &value) {
        EmplaceBack(value);
    }

    void PushBack(T &&value) {
        EmplaceBack(std::move(value));
    }

    template<typename... Args>
    T &EmplaceBack(Args &&... args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    }

private:
    static void UninitializedMoveOrCopyN(T *from, size_t count, T *to) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, count, to);
        } else {
            std::uninitialized_copy_n(from, count, to);
        }
    }

    template<typename... Args>
    iterator EmplaceWithRealloc(size_t index, Args &&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new(new_data.GetAddress() + index) T(std::forward<Args>(args)...);

        try {
            UninitializedMoveOrCopyN(data_.GetAddress(), index, new_data.GetAddress());
        } catch (...) {
            std::destroy_at(new_data.GetAddress() + index);
            throw;
        }
        try {
            UninitializedMoveOrCopyN(data_.GetAddress() + index, size_ - index,
                                     new_data.GetAddress() + index + 1);
        } catch (...) {
            std::destroy_n(new_data.GetAddress(), index + 1);
            throw;
        }

        DestroyN(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
        return begin() + index;
    }

    template<typename... Args>
    iterator EmplaceWithoutRealloc(size_t index, Args &&... args) {
        if (index == size_) {
            new(data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        } else {
            T tmp(std::forward<Args>(args)...);
            new(data_.GetAddress() + size_) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = std::move(tmp);
        }
        ++size_;
        return begin() + index;
    }

    static void DestroyN(T *ptr, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            (ptr + i)->~T();
        }
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
