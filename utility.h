#ifndef UTILITY_H
#define UTILITY_H

#include <iterator>

template <typename It, typename Category>
using has_category = std::is_convertible<typename std::iterator_traits<It>::iterator_category, Category>;

template <typename It>
constexpr bool is_input_iterator_v = has_category<It, std::input_iterator_tag>::value;

template <typename It>
constexpr bool is_forward_iterator_v = has_category<It, std::forward_iterator_tag>::value;

#endif // UTILITY_H
