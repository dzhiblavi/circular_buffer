#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cassert>

#include "compressed_pair.h"
#include "utility.h"

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
     *   data = [oldest_index_, capacity_) U [0, write_index mod capacity())
     *   &&
     *      0 <= oldest_index < capacity()
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

    static void destroy_(pointer storage, size_t capacity, size_t oi, size_t wi) noexcept {
        if (wi < capacity) {
            std::destroy(storage + oi, storage + wi);
        } else {
            wi %= capacity;
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

    template <typename It>
    It insert_impl_(It first, It last) {
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
     * Construct a buffer with specified capacity filled with data in range
     * @param first, last are expected to be input iterators
     *
     * This constructor takes only first initial_capacity elements from the range
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
        insert(first, last);
    }

    ~basic_circular_buffer() noexcept {
        if (!storage_)
            return;

        destroy_(storage_, capacity(), oldest_index_, write_index_);
    }

    /*
     * Inserts given range to the buffer while there is empty space for new elements
     * @param first, last -- range to be inserted
     *
     * This method does not overwrite existing data
     *
     * @throw any exception caused by copy-construction of T
     * @guarantee strong
     * */
    template <typename It
            , typename = std::enable_if_t<is_input_iterator_v<It>>>
    It insert(It first, It last) {
        return insert_impl_(first, last);
    }

    /*
     * Adds one element to the end of the buffer
     * @param value -- the element to be copied
     *
     * This method can overwtite data
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
        return size() == 0;
    }
};

typedef basic_circular_buffer<char> circular_buffer;

#endif // CIRCULAR_BUFFER_H
