#ifndef BASIC_CIRCULAR_BUFFER_H
#define BASIC_CIRCULAR_BUFFER_H

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cassert>

#include "compressed_pair.h"
#include "utility.h"

template <typename T, typename Traits, typename Pointer, typename Reference>
struct basic_circular_buffer_iterator_impl {
    typedef T value_type;
    typedef Reference reference;
    typedef Pointer pointer;

    typedef typename Traits::difference_type difference_type;
    typedef std::random_access_iterator_tag iterator_category;

    template<typename, typename> friend class basic_circular_buffer;
    template<typename, typename> friend class basic_circular_buffer_const_iterator;

private:
    pointer base_ = nullptr;
    size_t index_ = 0;
    size_t capacity_ = 0;

private:
    basic_circular_buffer_iterator_impl(pointer base, size_t index, size_t capacity)
        : base_(base)
        , index_(index)
        , capacity_(capacity) {}

    size_t checked_index_(size_t index) const noexcept {
        return index < capacity_ ? index_ : index_ - capacity_;
    }

public:
    basic_circular_buffer_iterator_impl() = default;
    basic_circular_buffer_iterator_impl(basic_circular_buffer_iterator_impl const&) = default;
    basic_circular_buffer_iterator_impl& operator=(basic_circular_buffer_iterator_impl const&) = default;

    basic_circular_buffer_iterator_impl& operator++() noexcept {
        ++index_;
        return *this;
    }

    basic_circular_buffer_iterator_impl& operator--() noexcept {
        if (index_ == 0)
            index_ = capacity_;
        --index_;

        return *this;
    }

    const basic_circular_buffer_iterator_impl operator++(int) noexcept {
        basic_circular_buffer_iterator_impl ret(*this);
        ++(*this);
        return ret;
    }

    const basic_circular_buffer_iterator_impl operator--(int) noexcept {
        basic_circular_buffer_iterator_impl ret(*this);
        --(*this);
        return ret;
    }

    bool operator==(basic_circular_buffer_iterator_impl const& other) {
        return index_ == other.index_;
    }

    bool operator!=(basic_circular_buffer_iterator_impl const& other) {
        return !(*this == other);
    }

    reference operator*() const {
        return base_[checked_index_(index_)];
    }

    pointer operator->() const {
        return base_ + checked_index_(index_);
    }

    reference operator[](size_t index) {
        return base_[checked_index_(index_ + index)];
    }

    bool operator<(basic_circular_buffer_iterator_impl const& other) {
        return index_ < other.index_;
    }

    bool operator>(basic_circular_buffer_iterator_impl const& other) {
        return other < *this;
    }

    bool operator<=(basic_circular_buffer_iterator_impl const& other) {
        return !(this > other);
    }

    bool operator>=(basic_circular_buffer_iterator_impl const& other) {
        return !(*this < other);
    }

    basic_circular_buffer_iterator_impl& operator+=(size_t shift) noexcept {
        index_ += shift;
        return *this;
    }

    basic_circular_buffer_iterator_impl& operator-=(size_t shift) noexcept {
        if (index_ < base_ + shift)
            index_ = base_ + capacity_ + 1;
        index_ -= shift;

        return *this;
    }

    friend difference_type operator-(basic_circular_buffer_iterator_impl const& a
                                   , basic_circular_buffer_iterator_impl const& b) {
        return a.index_ - b.index_;
    }
};

template <typename T, typename Traits>
using basic_circular_buffer_iterator
        = basic_circular_buffer_iterator_impl<T, Traits, typename Traits::pointer, T&>;

template <typename T, typename Traits>
using basic_circular_buffer_const_iterator
        = basic_circular_buffer_iterator_impl<T, Traits, typename Traits::const_pointer, T const&>;

template <typename T, typename Alloc = std::allocator<T>>
class basic_circular_buffer {
public:
    typedef T value_type;
    typedef T& reference;
    typedef T const& const_reference;

    typedef Alloc allocator_type;
    typedef std::allocator_traits<Alloc> alloc_traits;
    typedef typename alloc_traits::difference_type difference_type;
    typedef typename alloc_traits::size_type size_type;
    typedef typename alloc_traits::pointer pointer;
    typedef typename alloc_traits::const_pointer const_pointer;

    typedef basic_circular_buffer_iterator<T, alloc_traits> iterator;
    typedef basic_circular_buffer_const_iterator<T, alloc_traits> const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

private:
    struct unp_destructor_ {
        typedef Alloc allocator_type;
        typedef std::allocator_traits<Alloc> alloc_traits;

        typedef typename alloc_traits::size_type size_type;
        typedef typename alloc_traits::pointer pointer;

    private:
        allocator_type na_;
        size_t capacity_;

    public:
        explicit unp_destructor_(allocator_type& na, size_t capacity)
            : na_(na)
            , capacity_(capacity) {}

        void operator()(pointer p) noexcept {
            alloc_traits::deallocate(na_, p, capacity_);
        }
    };

    typedef std::unique_ptr<value_type, unp_destructor_> pointer_hold_;

private:
    compressed_pair<allocator_type, size_t> cpair_;
    pointer storage_ = nullptr;

    /*
     * I : main data structure invariant
     *
     * I = if write_index_ < capacity() then
     *   data = [oldest_index_, write_index_)
     *     else
     *   data = [oldest_index_, capacity_) U [0, write_index_ mod capacity())
     *   &&
     *      0 <= oldest_index_ < capacity()
     *   &&
     *      oldest_index_ <= write_index_
     * */
    size_t write_index_ = 0;
    size_t oldest_index_ = 0;

private:
    size_t& capacity_() noexcept {
        return cpair_.second();
    }

    size_t const& capacity_() const noexcept {
        return cpair_.second();
    }

    allocator_type& alloc_() noexcept {
        return cpair_.first();
    }

    allocator_type const& alloc_() const noexcept {
        return cpair_.first();
    }

    pointer_hold_ allocate_(size_t capacity) {
        return pointer_hold_(alloc_traits::allocate(alloc_(), capacity), unp_destructor_(alloc_(), capacity));
    }

    size_t checked_index_(size_t index) const noexcept {
        return index >= capacity() ? index - capacity() : index;
    }

    /*
     * Destructs object in 'storage regarding I
     * */
    static void destroy_(pointer storage, size_t capacity, size_t oi, size_t wi) noexcept {
        if (wi < capacity) {
            std::destroy(storage + oi, storage + wi);
        } else {
            wi -= capacity;
            std::destroy(storage + oi, storage + capacity);
            std::destroy(storage, storage + wi);
        }
    }

    template <typename It>
    void construct_impl_(It first, It last) {
        typedef typename It::difference_type it_difference_type;

        difference_type it_distance = std::distance(first, last);
        pointer_hold_ new_storage = allocate_(it_distance);

        std::uninitialized_copy(first, last, new_storage.get());

        capacity_() = it_distance;
        storage_ = new_storage.release();

        oldest_index_ = 0;
        write_index_ = capacity_();
    }

    /*
     * @guarantee strong
     * */
    template <typename It>
    It append_impl_(It first, It last) {
        size_t constructed = 0;
        size_t old_write = write_index_;

        try {
            while (first != last && size() < capacity()) {
                push_back(*first++);
                ++constructed;
            }

            return first;
        } catch (...) {
            destroy_(storage_, capacity(), old_write, write_index_);
            write_index_ = old_write;
            throw;
        }
    }

    template <typename... Args>
    bool push_back_impl_(Args&&... args) {
        if (capacity_() == 0) {
            return false;
        }

        bool overwrite = write_index_ >= capacity() && write_index_ - capacity() == oldest_index_;
        size_t construct_index = checked_index_(write_index_);

        if (overwrite) {
            ++oldest_index_;

            if (oldest_index_ == capacity()) {
                oldest_index_ = 0;
                write_index_ = capacity() - 1;
            }

            alloc_traits::destroy(alloc_(), storage_ + construct_index);
        }

        alloc_traits::construct(alloc_(), storage_ + construct_index, std::forward<Args>(args)...);

        ++write_index_;
        return overwrite;
    }

    /*
     * Copies all data from 'old_storage to 'new_storage regarding I
     *
     * Ordering may change. If 'new_capacity < 'old_size,
     *  then last 'new_capacity - 'old_size data would be lost
     *
     * @return write_index_ for new_storage. oldest_index_ is equal to 0
     * @gurarantee strong
     * */
    static size_t safe_data_copy_impl_(pointer new_storage, size_t new_capacity, basic_circular_buffer const& b) {
        pointer base = b.storage_ + b.oldest_index_;
        size_t size = std::min(new_capacity, b.size());

        if (b.oldest_index_ + size <= b.capacity()) {
            std::uninitialized_copy(base, base + size, new_storage);
        } else {
            pointer old_end = b.storage_ + b.capacity();
            std::uninitialized_copy(base, old_end, new_storage);
            size_t copied = old_end - base;

            try {
                std::uninitialized_copy(b.storage_, b.storage_ + (size - copied), new_storage + copied);
            } catch (...) {
                std::destroy(new_storage, new_storage + copied);
                throw;
            }
        }

        return size;
    }

    static size_t noexcept_data_move_impl_(pointer new_storage, size_t new_capacity, basic_circular_buffer const& b) {
        pointer base = b.storage_ + b.oldest_index_;
        size_t size = std::min(new_capacity, b.size());

        if (b.oldest_index_ + size <= b.capacity()) {
            std::move(base, base + size, new_storage);
        } else {
            pointer old_end = b.storage_ + b.capacity();
            std::move(base, old_end, new_storage);
            size_t copied = old_end - base;
            std::move(b.storage_, b.storage_ + (size - copied), new_storage + copied);
        }

        return size;
    }

    template <typename F
            , typename = std::enable_if_t<std::is_invocable_v<F, pointer, size_t, basic_circular_buffer const&>>>
    void resize_impl_(size_t capacity, F&& copy_method) {
        pointer_hold_ new_storage = allocate_(capacity);

        size_t wi = write_index_;
        size_t oi = oldest_index_;

        write_index_ = copy_method(new_storage.get(), capacity, *this);

        oldest_index_ = 0;

        destroy_(storage_, capacity_(), oi, wi);
        alloc_traits::deallocate(alloc_(), storage_, capacity_());

        storage_ = new_storage.release();
    }

    void resize_impl_(size_t capacity, std::true_type) {
        resize_impl_(capacity, noexcept_data_move_impl_);
    }

    void resize_impl_(size_t capacity, std::false_type) {
        resize_impl_(capacity, safe_data_copy_impl_);
    }

public:
    /*
     * Constructs an empty buffer
     * No memory allocation will be done
     *
     * After this, 'size = 'capacity = 0
     * */
    basic_circular_buffer() noexcept = default;

    /*
     * Constructs an empty buffer with pre-allocated storage
     * @param initial_capacity -- required buffer capacity
     *
     * After this, 'size == 0
     *             'capacity = initial_capacity
     *
     * @throw std::bad_alloc
     * */
    basic_circular_buffer(size_t initial_capacity)
        : storage_(allocate_(initial_capacity).release()) {
        capacity_() = initial_capacity;
    }

    /*
     * Constructs a buffer filled with data in range
     * @param first, last are expected to be forward iterators
     *
     * After this, 'size = 'capacity = std::distance(first, last)
     *
     * @throw std::bad_alloc
     * @throw any exception caused by copy-construction of T
     * */
    template <typename ForwardIt
            , typename = std::enable_if_t<is_forward_iterator_v<ForwardIt>>>
    basic_circular_buffer(ForwardIt first, ForwardIt last) {
        construct_impl_(first, last);
    }

    /*
     * Constructs a buffer with specified capacity filled with data in range
     * @param first, last are expected to be input iterators
     *
     * This constructor takes only first 'initial_capacity elements from the range
     *
     * After this, 'capacity = initial_capacity
     *             'size = min('capacity, std::distance(first, last))
     *
     * @throw std::bad_alloc
     * @throw any exception caused by copy-construction of T
     * */
    template <typename It
            , typename = std::enable_if_t<is_input_iterator_v<It>>>
    basic_circular_buffer(size_t initial_capacity, It first, It last)
        : basic_circular_buffer(initial_capacity) {
        append(first, last);
    }

    ~basic_circular_buffer() noexcept {
        if (!storage_)
            return;

        destroy_(storage_, capacity(), oldest_index_, write_index_);
        alloc_traits::deallocate(alloc_(), storage_, capacity());
    }

    /*
     * Creates an exact copy of 'other buffer
     *
     * After this, 'size() = 'other.size()
     *             'capacity() = 'other.capacity()
     *             and all elements in range [0, size() - 1) are equal
     *             in both buffers respectively
     *
     * @param buffer to be copied
     * @throw std::bad_alloc
     * @throw any exception caused by copy-construction of T
     * */
    basic_circular_buffer(basic_circular_buffer const& other) {
        pointer_hold_ new_storage = allocate_(other.capacity());

        write_index_ = safe_data_copy_impl_(new_storage.get(), other.capacity(), other);

        oldest_index_ = 0;
        capacity_() = other.capacity();
        storage_ = new_storage.release();
    }

    /*
     * Move-constructor
     *
     * Calling this constructor results 'other to be defaulted
     *
     * @param buffer to be moved from
     * */
    basic_circular_buffer(basic_circular_buffer&& other) noexcept {
        swap(other);
    }

    /*
     * Copy-assignment operator
     *
     * After this, 'size() = 'other.size()
     *             'capacity() must not be equal to 'other.capacity()
     *             and all elements in range [0, size() - 1) are equal
     *             in both buffers respectively
     *
     * @param buffer to be copied
     * @throw std::bad_alloc
     * @throw any exception caused by copy-construction of T
     * @guarantee strong, if capacity() < other.size()
     *            basic, otherwise
     * */
    basic_circular_buffer& operator=(basic_circular_buffer const& other) {
        if (this == &other)
            return *this;

        if (capacity() < other.size()) {
            basic_circular_buffer tmp(other);
            swap(tmp);
        } else {
            destroy_(storage_, capacity(), oldest_index_, write_index_);

            write_index_ = 0;
            oldest_index_ = 0;
            write_index_ = safe_data_copy_impl_(storage_, capacity(), other);
        }

        return *this;
    }

    /*
     * Move-assignment operator
     *
     * Calling this operator results 'other to be defaulted
     *
     * @param buffer to be moved from
     * @return reference to current buffer
     * */
    basic_circular_buffer& operator=(basic_circular_buffer&& other) noexcept {
        if (this == &other)
            return *this;

        swap(other);
        return *this;
    }

    void swap(basic_circular_buffer& other) noexcept {
        std::swap(cpair_, other.cpair_);
        std::swap(storage_, other.storage_);
        std::swap(write_index_, other.write_index_);
        std::swap(oldest_index_, other.oldest_index_);
    }

    /*
     * appends given range to the end of the buffer while there is empty space for new elements
     * @param first, last -- range to be appended
     *
     * This method does not overwrite existing data
     *
     * @throw any exception caused by copy-construction of T
     * @guarantee strong
     * */
    template <typename It
            , typename = std::enable_if_t<is_input_iterator_v<It>>>
    It append(It first, It last) {
        return append_impl_(first, last);
    }

    /*
     * Adds one element to the end of the buffer
     * @param value -- the element to be copied
     *
     * This method can overwrite data
     *
     * @return if existing element was overwritten or not
     * @throw any exception caused by copy-construction or destruction of T
     * @guarantee basic if size() == capacity()
     *            strong otherwise
     * */
    bool push_back(const_reference value) {
        return push_back_impl_(value);
    }

    /*
     * Adds one element to the end of the buffer
     * @param value -- the element to be moved
     *
     * This method can overwrite data
     *
     * @return if existing element was overwritten or not
     * @throw any exception caused by move-construction or destruction of T
     * @guarantee basic if size() == capacity()
     *            strong otherwise
     * */
    bool push_back(value_type&& value) {
        return push_back_impl_(std::move(value));
    }

    /*
     * Adds one element to the end of the buffer
     * @param args... -- arguments to construct T of
     *
     * This method can overwrite data
     *
     * @return if existing element was overwritten or not
     * @throw any exception caused by construction or desctruction of T
     * @guarantee basic if size() == capacity()
     *            strong otherwise
     * */
    template <typename... Args>
    bool emplace_back(Args&&... args) {
        return push_back_impl_(std::forward<Args>(args)...);
    }

    /*
     * Removes the first element
     *
     * After this, 'size() = 'size() - 1
     *
     * If the buffer is empty, behaviour is undefined
     * */
    void pop_front() noexcept {
        alloc_traits::destroy(alloc_(), storage_ + oldest_index_);
        ++oldest_index_;

        if (oldest_index_ == capacity()) {
            oldest_index_ = 0;
            write_index_ -= capacity();
        }
    }

    /*
     * Removes the last element
     *
     * After this, 'size() = 'size() - 1
     *
     * If the buffer is empty, behaviour is undefined
     * */
    void pop_back() noexcept {
        alloc_traits::destroy(alloc_(), storage_ + checked_index_(write_index_ - 1));
        --write_index_;
    }

    /*
     * Changes capacity
     *
     * @param capacity
     *
     * If capacity < size(), then last capacity - size() data would be lost
     *
     * After this, 'capacity() = capacity
     *
     * @throw std::bad_alloc
     * @throw any exception caused by copy-construction of T
     * @guarantee strong
     * */
    void resize(size_t capacity) {
        resize_impl_(capacity, std::is_nothrow_move_constructible<T>());
    }

    /*
     * Access the first element of the buffer
     *
     * If the buffer is empty, behaviour is undefined
     *
     * @return reference to the first element
     * */
    reference front() noexcept {
        return storage_[oldest_index_];
    }

    const_reference front() const noexcept {
        return storage_[oldest_index_];
    }

    /*
     * Access the last element of the buffer
     *
     * If the buffer is empty, behaviour is undefined
     *
     * @return reference to the last element
     * */
    reference back() noexcept {
        return (*this)[size() - 1];
    }

    const_reference back() const noexcept {
        return (*this)[size() - 1];
    }

    /*
     * Access the buffer via index
     * @param index
     *
     * If index is out of range [0, size()), behaviour is undefined
     *
     * @return reference to i-th element of the buffer
     * */
    reference operator[](size_t index) noexcept {
        return storage_[checked_index_(oldest_index_ + index)];
    }

    const_reference operator[](size_t index) const noexcept {
        return storage_[checked_index_(oldest_index_ + index)];
    }

    /*
     * Member functions used to get iterators
     *
     * Any modifying operation invalidates all iterators
     *
     * All iterators implement random access interface
     * */
    iterator begin() noexcept {
        return iterator(storage_, oldest_index_, capacity());
    }

    iterator end() noexcept {
        return iterator(storage_, write_index_, capacity());
    }

    const_iterator begin() const noexcept {
        return const_iterator(storage_, oldest_index_, capacity());
    }

    const_iterator end() const noexcept {
        return const_iterator(storage_, write_index_, capacity());
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    reverse_iterator rbegin() noexcept {
        return reverse_iterator(end());
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(cend());
    }

    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(cbegin());
    }

    const_reverse_iterator rcbegin() const noexcept {
        return rbegin();
    }

    const_reverse_iterator rcend() const noexcept {
        return rend();
    }

    allocator_type get_allocator() const noexcept {
        return alloc_();
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_();
    }

    [[nodiscard]] size_t size() const noexcept {
        return write_index_ >= oldest_index_ ?
               write_index_ - oldest_index_ : capacity_() - oldest_index_ + write_index_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return write_index_ == 0;
    }
};

#endif // BASIC_CIRCULAR_BUFFER_H
