#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "basic_circular_buffer.h"

/*
 * Threadsafe basic_circular_buffer
 * */
template<typename T, typename Alloc = std::allocator<T>>
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
    base_ *base() noexcept {
        return static_cast<base_ *>(this);
    }

    base_ const *base() const noexcept {
        return static_cast<base_ const *>(this);
    }

public:
    /*
     * @see basic_circular_buffer
     * */
    circular_buffer() noexcept = default;

    /*
     * @see basic_circular_buffer
     * */
    circular_buffer(size_t initial_capacity)
            : base_(initial_capacity) {
    }

    /*
     * @see basic_circular_buffer
     * */
    template<typename ForwardIt>
    circular_buffer(ForwardIt first, ForwardIt last)
            : base_(first, last) {
    }

    /*
     * @see basic_circular_buffer
     * */
    template<typename It>
    circular_buffer(It first, It last, size_t initial_capacity)
            : base_(first, last, initial_capacity) {
    }

    /*
     * @see basic_circular_buffer
     * */
    ~circular_buffer() noexcept = default;

    /*
     * @see basic_circular_buffer
     * */
    circular_buffer(circular_buffer const &other)
            : base_(other) {
    }

    /*
     * @see basic_circular_buffer
     * */
    circular_buffer& operator=(circular_buffer const &other) {
        std::scoped_lock<std::mutex, std::mutex> lg(m_, other.m_);
        *base() = other;

        return *this;
    }

    /*
     * @see basic_circular_buffer
     * */
    circular_buffer(circular_buffer &&other) {
        swap(other);
    }

    /*
     * @see basic_circular_buffer
     * */
    circular_buffer& operator=(circular_buffer &&other) {
        swap(other);

        return *this;
    }

    /*
     * @see basic_circular_buffer
     * */
    void swap(circular_buffer &other) {
        std::scoped_lock<std::mutex, std::mutex> lg(m_, other.m_);
        base()->swap(other);
    }

    /*
     * @see basic_circular_buffer
     *
     * Call of this method may unlock 'wait method
     * */
    template<typename It>
    void append(It first, It last) {
        std::lock_guard<std::mutex> lg(m_);
        base()->append(first, last);
        cv_.notify_all();
    }

    /*
     * @see basic_circular_buffer
     *
     * Call of this method may unlock 'wait method
     * */
    void push_back(const_reference value) {
        std::lock_guard<std::mutex> lg(m_);
        base()->push_back(value);
        cv_.notify_one();
    }

    /*
     * @see basic_circular_buffer
     *
     * Call of this method may unlock 'wait method
     * */
    void push_back(value_type &&value) {
        std::lock_guard<std::mutex> lg(m_);
        base()->push_back(std::move(value));
        cv_.notify_one();
    }

    /*
     * @see basic_circular_buffer
     *
     * Call of this method may unkock 'wait method
     * */
    template<typename... Args>
    void emplace_back(Args &&... args) {
        std::lock_guard<std::mutex> lg(m_);
        base()->emplace_back(std::forward<Args>(args)...);
        cv_.notify_one();
    }

    /*
     * Threaded version of pop_front() from basic_circular_buffer
     *
     * This method blocks thread execution until buffer becomes non-empty
     *  then it effectively returns front() and performs pop_front()
     *
     * @param value -- value to be stored in
     * @throws any exception caused by move-assignment of T
     * @guarantee basic
     * */
    void wait_pop(T &value) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        cv_.wait(lg, [this] {
            return !base()->empty();
        });

        try {
            value = std::move(base()->front());
        } catch (...) {
            base()->pop_front();
            throw;
        }

        base()->pop_front();
    }

    /*
     * Threaded version of pop_front() from basic_circular_buffer
     *
     * This method blocks thread execution until buffer becomes non-empty
     *  then it effectively returns front() and performs pop_front()
     *
     * @return shared pointer to the front element
     * @throws any exception caused by move-construction of T
     * @guarantee basic
     * */
    std::shared_ptr<T> wait_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        cv_.wait(lg, [this] {
            return !base()->empty();
        });

        std::shared_ptr<T> ret;
        try {
            ret = std::make_shared<T>(std::move(base()->front()));
        } catch (...) {
            base()->pop_front();
            throw;
        }

        base()->pop_front();

        return ret;
    }

    /*
     * Threaded version of pop_front() from basic_circular_buffer
     *
     * This method does NOT block current thread execution
     *
     * If the buffer is empty, nothing is done
     *
     * @param value -- value to be stored in
     * @throws any exception caused by move-assignment of T
     * @guarantee basic
     * */
    bool try_pop(T &value) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        if (base()->empty()) {
            return false;
        }

        try {
            value = std::move(base()->front());
        } catch (...) {
            base()->pop_front();
            throw;
        }

        base()->pop_front();

        return true;
    }

    /*
     * Threaded version of pop_front() from basic_circular_buffer
     *
     * This method does NOT block current thread execution
     *
     * If the buffer is empty, nothing is done
     *
     * @return shared pointer to the front element
     * @throws any exception caused by move-construction of T
     * @guarantee basic
     * */
    std::shared_ptr<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        std::lock_guard<std::mutex> lg(m_);

        if (base()->empty()) {
            return {};
        }

        std::shared_ptr ret = std::make_shared<T>(std::move(base()->front()));
        base()->pop_front();

        return ret;
    }

    /*
     * This method gets not more than @param 'count and not less than one first elements,
     *   puts them into @param [first...) range and removes them from the buffer
     *
     * This method blocks thread execution until the buffer becomes non-empty
     *
     * @return iterator to the element after the last element read
     * @throws any exception caused by move-assignment of T
     * @guarantee basic
     * */
    template<typename OutputIt, typename = std::enable_if_t<is_output_iterator_v<OutputIt>>>
    OutputIt wait_npop(OutputIt first, size_t count) {
        std::lock_guard<std::mutex> lg(m_);
        size_t read = 0;

        cv_.wait(lg, [this] {
            return !base()->empty();
        });

        while (read < count && !base()->empty()) {
            try {
                first = std::move(base()->front());
            } catch (...) {
                base()->pop_front();
                throw;
            }

            base()->pop_front();
            ++first;
        }

        return first;
    }

    /*
     * @see basic_circular_buffer
     *
     * @return pair <size, lock>
     *
     * Where the lock is used to guarantee correct information
     * */
    std::pair<size_t, std::unique_lock<std::mutex>> size() const noexcept {
        std::unique_lock<std::mutex> lg(m_);

        return {base()->size(), std::move(lg)};
    }

    /*
     * @see basic_circular_buffer
     *
     * @return pair <size, lock>
     *
     * Where the lock is used to guarantee correct information
     * */
    std::pair<size_t, std::unique_lock<std::mutex>> capacity() const noexcept {
        std::unique_lock<std::mutex> lg(m_);

        return {base()->capacity(), std::move(lg)};
    }

    /*
     * @see basic_circular_buffer
     *
     * @return pair <size, lock>
     *
     * Where the lock is used to guarantee correct information
     * */
    std::pair<bool, std::unique_lock<std::mutex>> empty() const noexcept {
        std::unique_lock<std::mutex> lg(m_);

        return {base()->empty(), std::move(lg)};
    }

    allocator_type get_allocator() const noexcept {
        return base()->get_allocator();
    }

    /*
     * @see basic_circular_buffer
     * */
    void resize(size_t capacity) {
        std::lock_guard<std::mutex> lg(m_);
        base()->resize(capacity);
    }

    /*
     * Locks inner structure of the buffer
     *
     * @return std::unique_lock representing storage ownage
     *
     * Typical usage example:
     *     circular_buffer buffer;
     *     ...
     *
     *     {
     *         // start the buffer ownage
     *         buffer.lock();
     *
     *         for (auto const& x : buffer) {
     *             // buffer can be accessed safely
     *         }
     *     }
     * */
    std::unique_lock<std::mutex> lock() noexcept {
        return std::unique_lock<std::mutex>(m_);
    }

    /*
     * Iterators
     *
     * Iterators do not provide object ownership
     *  (their usage is not thread safe automatically)
     *
     * @see basic_circular_buffer
     * */
    iterator begin() noexcept {
        return base()->begin();
    }

    iterator end() noexcept {
        return base()->end();
    }

    const_iterator begin() const noexcept {
        return base()->begin();
    }

    const_iterator end() const noexcept {
        return base()->end();
    }

    const_iterator cbegin() const noexcept {
        return base()->cbegin();
    }

    const_iterator cend() const noexcept {
        return base()->cend();
    }

    reverse_iterator rbegin() noexcept {
        return base()->rbegin();
    }

    reverse_iterator rend() noexcept {
        return base()->rend();
    }

    const_reverse_iterator rbegin() const noexcept {
        return base()->rbegin();
    }

    const_reverse_iterator rend() const noexcept {
        return base()->rend();
    }

    const_reverse_iterator rcbegin() const noexcept {
        return base()->rcbegin();
    }

    const_reverse_iterator rcend() const noexcept {
        return base()->rcend();

    };
};

#endif // CIRCULAR_BUFFER_H
