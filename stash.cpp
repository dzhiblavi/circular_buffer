template <typename T, typename Traits>
struct basic_circular_buffer_iterator {
    typedef T value_type;
    typedef T& reference;
    typedef typename Traits::difference_type difference_type;
    typedef typename Traits::pointer pointer;
    typedef std::random_access_iterator_tag iterator_category;

    template<typename, typename> friend class basic_circular_buffer;
    template<typename, typename> friend class basic_circular_buffer_const_iterator;

private:
    pointer base_ = nullptr;
    size_t index_ = 0;
    size_t capacity_ = 0;

private:
    basic_circular_buffer_iterator(pointer base, size_t index, size_t capacity)
            : base_(base)
            , index_(index)
            , capacity_(capacity) {}

    size_t checked_index_(size_t index) const noexcept {
        return index < capacity_ ? index_ : index_ - capacity_;
    }

public:
    basic_circular_buffer_iterator() = default;
    basic_circular_buffer_iterator(basic_circular_buffer_iterator const&) = default;

    basic_circular_buffer_iterator& operator=(basic_circular_buffer_iterator const&) = default;

    basic_circular_buffer_iterator& operator++() noexcept {
        ++index_;
        return *this;
    }

    basic_circular_buffer_iterator& operator--() noexcept {
        if (index_ == 0)
            index_ = capacity_;
        --index_;

        return *this;
    }

    const basic_circular_buffer_iterator operator++(int) noexcept {
        basic_circular_buffer_iterator ret(*this);
        ++(*this);
        return ret;
    }

    const basic_circular_buffer_iterator operator--(int) noexcept {
        basic_circular_buffer_iterator ret(*this);
        --(*this);
        return ret;
    }

    bool operator==(basic_circular_buffer_iterator const& other) {
        return index_ == other.index_;
    }

    bool operator!=(basic_circular_buffer_iterator const& other) {
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

    bool operator<(basic_circular_buffer_iterator const& other) {
        return index_ < other.index_;
    }

    bool operator>(basic_circular_buffer_iterator const& other) {
        return other < *this;
    }

    bool operator<=(basic_circular_buffer_iterator const& other) {
        return !(this > other);
    }

    bool operator>=(basic_circular_buffer_iterator const& other) {
        return !(*this < other);
    }

    basic_circular_buffer_iterator& operator+=(size_t shift) noexcept {
        index_ += shift;
        return *this;
    }

    basic_circular_buffer_iterator& operator-=(size_t shift) noexcept {
        if (index_ < base_ + shift)
            index_ = base_ + capacity_ + 1;
        index_ -= shift;

        return *this;
    }

    friend difference_type operator-(basic_circular_buffer_iterator const& a
                                   , basic_circular_buffer_iterator const& b) {
        return a.index_ - b.index_;
    }
};

template <typename T, typename Traits>
struct basic_circular_buffer_const_iterator {
    typedef T value_type;
    typedef T const& reference;
    typedef typename Traits::difference_type difference_type;
    typedef typename Traits::const_pointer pointer;
    typedef std::random_access_iterator_tag iterator_category;

    template<typename, typename> friend class basic_circular_buffer;

private:
    pointer base_ = nullptr;
    size_t index_ = 0;
    size_t capacity_ = 0;

private:
    basic_circular_buffer_const_iterator(pointer base, size_t index, size_t capacity)
        : base_(base)
        , index_(index)
        , capacity_(capacity) {}

    size_t checked_index_(size_t index) const noexcept {
        return index < capacity_ ? index_ : index_ - capacity_;
    }

public:
    basic_circular_buffer_const_iterator() = default;
    basic_circular_buffer_const_iterator(basic_circular_buffer_const_iterator const&) = default;

    basic_circular_buffer_const_iterator(basic_circular_buffer_iterator<T, Traits> const& other)
        : base_(other.base_)
        , index_(other.index_)
        , capacity_(other.capacity_) {}

    basic_circular_buffer_const_iterator& operator=(basic_circular_buffer_const_iterator const&) = default;

    basic_circular_buffer_const_iterator& operator++() noexcept {
        ++index_;
        return *this;
    }

    basic_circular_buffer_const_iterator& operator--() noexcept {
        if (index_ == 0)
            index_ = capacity_;
        --index_;

        return *this;
    }

    const basic_circular_buffer_const_iterator operator++(int) noexcept {
        basic_circular_buffer_iterator ret(*this);
        ++(*this);
        return ret;
    }

    const basic_circular_buffer_const_iterator operator--(int) noexcept {
        basic_circular_buffer_iterator ret(*this);
        --(*this);
        return ret;
    }

    bool operator==(basic_circular_buffer_const_iterator const& other) {
        return index_ == other.index_;
    }

    bool operator!=(basic_circular_buffer_const_iterator const& other) {
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

    bool operator<(basic_circular_buffer_const_iterator const& other) {
        return index_ < other.index_;
    }

    bool operator>(basic_circular_buffer_const_iterator const& other) {
        return other < *this;
    }

    bool operator<=(basic_circular_buffer_const_iterator const& other) {
        return !(this > other);
    }

    bool operator>=(basic_circular_buffer_const_iterator const& other) {
        return !(*this < other);
    }

    basic_circular_buffer_const_iterator& operator+=(size_t shift) noexcept {
        index_ += shift;
        return *this;
    }

    basic_circular_buffer_const_iterator& operator-=(size_t shift) noexcept {
        if (index_ < base_ + shift)
            index_ = base_ + capacity_ + 1;
        index_ -= shift;

        return *this;
    }

    friend difference_type operator-(basic_circular_buffer_const_iterator const& a
                                   , basic_circular_buffer_const_iterator const& b) {
        return a.index_ - b.index_;
    }
};