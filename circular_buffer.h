#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "basic_circular_buffer.h"

/*
 * A threadsafe version of basic_circular_buffer
 * */
template <typename T, typename Alloc = std::allocator<T>>
class circular_buffer : private basic_circular_buffer<T, Alloc> {
    typedef basic_circular_buffer<T, Alloc> base_;

public:
    typedef typename base_::value_type value_type;
    typedef typename base_::reference reference;
    typedef typename base_::const_reference const_reference;

    typedef typename base_::allocator_type allocator_type;
    typedef typename base_::alloc_traits alloc_traits;
    typedef typename base_::difference_type difference_type;
    typedef typename base_::size_type size_type;
    typedef typename base_::pointer pointer;
    typedef typename base_::const_pointer const_pointer;

    typedef typename base_::iterator iterator;
    typedef typename base_::const_iterator const_iterator;
    typedef typename base_::reverse_iterator reverse_iterator;
    typedef typename base_::const_reverse_iterator const_reverse_iterator;

private:
    mutable std::mutex m_;
    std::condition_variable cv_;

private:
    base_* base() noexcept {
        return static_cast<base_*>(this);
    }

    base_ const* base() const noexcept {
        return static_cast<base_ const*>(this);
    }

public:
    circular_buffer() noexcept = default;

    circular_buffer(size_t initial_capacity)
        : base_(initial_capacity) {
    }

    template <typename ForwardIt>
    circular_buffer(ForwardIt first, ForwardIt last)
        : base_(first, last) {
    }

    template <typename It>
    circular_buffer(It first, It last, size_t initial_capacity)
        : base_(first, last, initial_capacity) {
    }

    ~circular_buffer() = default;

    circular_buffer(circular_buffer const& other) {
        std::scoped_lock<std::mutex, std::mutex> lg(m_, other.m_);
        *static_cast<base_*>(this)(other);
    }

    circular_buffer& operator=(circular_buffer const& other) {
        std::scoped_lock<std::mutex, std::mutex> lg(m_, other.m_);

        return *static_cast<base_*>(this) = other;
    }

    circular_buffer(circular_buffer&& other) {
        swap(other);
    }

    circular_buffer& operator=(circular_buffer&& other) {
        swap(other);

        return *this;
    }

    void swap(circular_buffer& other) {
        std::scoped_lock<std::mutex, std::mutex> lg(m_, other.m_);
        base()->swap(other);
    }

    template <typename It>
    void append(It first, It last) {
        std::lock_guard<std::mutex> lg(m_);
        base()->append(first, last);
        cv_.notify_all();
    }

    void push_back(const_reference value) {
        std::lock_guard<std::mutex> lg(m_);
        base()->push_back(value);
        cv_.notify_one();
    }

    void push_back(value_type&& value) {
        std::lock_guard<std::mutex> lg(m_);
        base()->push_back(std::move(value));
        cv_.notify_one();
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        std::lock_guard<std::mutex> lg(m_);
        base()->emplace_back(std::forward<Args>(args)...);
        cv_.notify_one();
    }

    void wait_pop(T& value) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        cv_.wait(lg, [this] {
            return !base()->empty();
        });

        value = std::move(base()->front());
        base()->pop_front();
    }

    std::shared_ptr<T> wait_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        cv_.wait(lg, [this] {
            return !base()->empty();
        });

        std::shared_ptr<T> ret = std::make_shared<T>(std::move(base()->front()));
        base()->pop_front();

        return ret;
    }

    bool try_pop(T& value) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        if (base()->empty()) {
            return false;
        }

        value = std::move(base()->front());
        base()->pop_front();

        return true;
    }

    std::shared_ptr<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        if (base()->empty()) {
            return {};
        }

        std::shared_ptr ret = std::make_shared<T>(std::move(base()->front()));
        base()->pop_front();

        return ret;
    }

    template <typename OutputIt
            , typename = std::enable_if_t<is_output_iterator_v<OutputIt>>>
    size_t npop(OutputIt first, size_t count) {
        std::lock_guard<std::mutex> lg(m_);
        size_t read = 0;

        cv_.wait(lg, [this] {
            return !base()->empty();
        });

        for (; read < count && !base()->empty(); ++read, ++first) {
            first = std::move(base()->front());
            base()->pop_front();
        }

        return read;
    }

    std::pair<size_t, std::unique_lock<std::mutex>> size() const noexcept {
        std::unique_lock<std::mutex> lg(m_);

        return {base()->size(), std::move(lg)};
    }

    std::pair<size_t, std::unique_lock<std::mutex>> capacity() const noexcept {
        std::unique_lock<std::mutex> lg(m_);

        return {base()->capacity(), std::move(lg)};
    }

    std::pair<bool, std::unique_lock<std::mutex>> empty() const noexcept {
        std::unique_lock<std::mutex> lg(m_);

        return {base()->empty(), std::move(lg)};
    }

    allocator_type get_allocator() const noexcept {
        return base()->get_allocator();
    }

    void resize(size_t capacity) {
        std::lock_guard<std::mutex> lg(m_);
        base()->resize(capacity);
    }
};

#endif // CIRCULAR_BUFFER_H
