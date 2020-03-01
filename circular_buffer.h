#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "basic_circular_buffer.h"

/*
 * A threadsafe (and reentrant?) version of basic_circular_buffer
 * */
template <typename T, typename Alloc>
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

public:

};

#endif // CIRCULAR_BUFFER_H
