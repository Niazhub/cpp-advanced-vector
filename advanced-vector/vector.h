#include <iostream>
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

using namespace std;

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity)
    {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(nullptr), capacity_(0)
    {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_()
        , size_(0)
    {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> temp(rhs);
                Swap(temp);
            }
            else {
                for (size_t i = 0; i < size_ && i < rhs.size_; ++i) {
                    data_[i] = rhs.data_[i];
                }

                if (size_ < rhs.size_) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                else if (size_ > rhs.size_) {
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }

                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size > data_.Capacity()) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            ReserveEmplaceBack((size_ == 0) ? 1 : size_ * 2, std::forward<Args>(args)...);
        }
        else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t index = pos - begin();
        iterator new_pos = const_cast<iterator>(pos);
        if (pos != end()) {
            if (size_ == data_.Capacity()) {
                ReserveEmplace((size_ == 0) ? 1 : size_ * 2, new_pos, std::forward<Args>(args)...);
            }
            else {
                new (end()) T(move(*end()));
                std::move_backward(new_pos, end() - 1, end());
                *new_pos = T(forward<Args>(args)...);
            }
            ++size_;
        }
        else {
            EmplaceBack(std::forward<Args>(args)...);
        }
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) {
        iterator new_pos = const_cast<iterator>(pos);
        std::move(new_pos + 1, end(), new_pos);
        std::destroy(data_.GetAddress() + size_ - 1, end());
        --size_;
        return (pos != end()) ? new_pos : end();
    }

    void PopBack() {
        if (size_ > 0) {
            destroy_n(data_.GetAddress() + size_ - 1, 1);
            --size_;
        }
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
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
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    template<typename... Args>
    void ReserveEmplaceBack(size_t new_capacity, Args&&... args) {
        RawMemory<T> new_data(new_capacity);

        new (new_data + size_) T(forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    template<typename... Args>
    void ReserveEmplace(size_t new_capacity, iterator new_pos, Args&&... args) {
        RawMemory<T> new_data(new_capacity);
        size_t index = new_pos - begin();

        new (new_data + index) T(forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), new_pos - data_.GetAddress(), new_data.GetAddress());
            std::uninitialized_move_n(new_pos, data_ + size_ - new_pos, new_data.GetAddress() + (new_pos - data_.GetAddress() + 1));
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), new_pos - data_.GetAddress(), new_data.GetAddress());
            std::uninitialized_copy_n(new_pos, data_.GetAddress() + size_ - new_pos, new_data.GetAddress() + (new_pos - data_.GetAddress() + 1));
        }

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }
};