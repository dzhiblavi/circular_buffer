#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

#include "compressed_pair.h"

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
    template <typename TT, typename TAlloc>
    struct unp_destructor_ {
        typedef TAlloc allocator_type;
        typedef std::allocator_traits<TAlloc> alloc_traits;

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

    typedef std::unique_ptr<value_type, unp_destructor_<T, Alloc>> pointer_hold_;

private:
    compressed_pair<allocator_type, size_t> cpair_;
    pointer storage_ = nullptr;

    /*
     * I = if write_index_ >= oldest_index_ then
     *   data = [oldest_index_, write_index_)
     *     else
     *   data = [oldest_index_, capacity_) U [0, write_index)
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
        return pointer_hold_(alloc_traits::allocate(alloc_(), capacity), unp_destructor_(alloc_()));
    }

    template <typename RaIt>
    void construct_impl_(RaIt first, RaIt last, std::forward_iterator_tag) {

    }

    template <typename InputIt>
    void construct_impl_(InputIt first, InputIt last, std::input_iterator_tag) {

    }

public:
    /*
     * Constructs an empty buffer
     * No memory allocation will be done
     * */
    basic_circular_buffer() noexcept = default;

    /*
     * Constructs an empty buffer with pre-allocated storage
     * @param initial_capacity : sizeof(T) * initial_capacity memory will be allocated
     * */
    basic_circular_buffer(size_t initial_capacity)
        : storage_(allocate_(initial_capacity)) {
        capacity_() = initial_capacity;
    }

    template <typename It>
    basic_circular_buffer(It first, It last) {
        construct_impl_(first, last, std::iterator_traits<It>::iterator_category());
    }

    ~basic_circular_buffer() {
        alloc_traits::deallocate(alloc_(), storage_, capacity_());
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
