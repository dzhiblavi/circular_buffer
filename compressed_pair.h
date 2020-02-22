#ifndef COMPRESSED_PAIR_HPP
#define COMPRESSED_PAIR_HPP

#include <utility>

template <typename T, bool = std::is_empty_v<T>>
class compressed_pair_base_ {
public:
    typedef T value_type;
    typedef T& reference;
    typedef T const& const_reference;

private:
    T value_;

public:
    template <typename =
        std::enable_if_t<std::is_default_constructible_v<T>>>
    compressed_pair_base_()
    noexcept(std::is_nothrow_default_constructible_v<T>)
        : value_() {}

    template <typename U, typename = typename
        std::enable_if_t<std::is_constructible_v<T, U&&>>>
    compressed_pair_base_(U&& x)
    noexcept(std::is_nothrow_constructible<T, U&&>::value)
            : value_(std::forward<U>(x))
    {}

    reference get() noexcept {
        return value_;
    }

    const_reference get() const noexcept {
        return value_;
    }
};

template <typename T>
class compressed_pair_base_<T, true>
        : private T {
public:
    typedef T value_type;
    typedef T& reference;
    typedef T const& const_reference;

public:
    template <typename =
        std::enable_if_t<std::is_default_constructible_v<T>>>
    compressed_pair_base_()
    noexcept(std::is_nothrow_default_constructible_v<T>)
        : T() {}

    template <typename U, typename = typename
        std::enable_if_t<std::is_constructible_v<T, U&&>>>
    compressed_pair_base_(U&& x)
    noexcept(std::is_nothrow_constructible<T, U&&>::value)
            : T(std::forward<U>(x))
    {}

    reference get() noexcept {
        return *this;
    }

    const_reference get() const noexcept {
        return *this;
    }
};

template <typename T1, typename T2>
class compressed_pair
        : private compressed_pair_base_<T1>
        , private compressed_pair_base_<T2> {
private:
    typedef compressed_pair_base_<T1> base1_;
    typedef compressed_pair_base_<T2> base2_;

public:
    typedef T1 first_type;
    typedef T2 second_type;

public:
    template<typename U1, typename U2>
    compressed_pair(U1&& x, U2&& y)
            : base1_(std::forward<U1>(x))
            , base2_(std::forward<U2>(y))
    {}

    compressed_pair() = default;
    ~compressed_pair() = default;
    compressed_pair(compressed_pair const&) = default;
    compressed_pair(compressed_pair&&) = default;
    compressed_pair &operator=(compressed_pair const&) = default;
    compressed_pair &operator=(compressed_pair&&) = default;

    typename base1_::reference first() noexcept {
        return static_cast<base1_*>(this)->get();
    }

    typename base1_::const_reference first() const noexcept {
        return static_cast<base1_ const*>(this)->get();
    }

    typename base2_::reference second() noexcept {
        return static_cast<base2_*>(this)->get();
    }

    typename base2_::const_reference second() const noexcept {
        return static_cast<base2_ const*>(this)->get();
    }
};

#endif // COMPRESSED_PAIR_HPP
