#include "gtest/gtest.h"

#include "circular_buffer.h"
#include "counted.h"
#include "fault_injection.h"

typedef basic_circular_buffer<counted> counted_buffer;

std::vector<counted> vec(size_t size) {
    std::vector<counted> ret;

    for (size_t i = 0; i < size; ++i)
        ret.emplace_back(i);

    return ret;
}

void trace(std::ostream&) {
}

template <typename U, typename... Us>
void trace(std::ostream& os, U const& u, Us&&... us) {
    os << "{";
    for (size_t i = 0; i < u.size(); ++i) {
        os << u[i];
        if (i < u.size() - 1)
            os << ", ";
    }
    os << "}" << std::endl;

    trace(os, us...);
}

template <typename U, typename V>
bool same(U const& u, V const& v) {
    if (u.size() != v.size())
        return false;

    for (size_t i = 0; i < u.size(); ++i)
        if (u[i] != v[i])
            return false;

    return true;
}

TEST(correctness, default_constructor) {
    faulty_run([] {
        counted::no_new_instances_guard guard;

        counted_buffer buffer;

        EXPECT_EQ(0, buffer.size());
        EXPECT_EQ(true, buffer.empty());
        EXPECT_EQ(0, buffer.capacity());

        guard.expect_no_instances();
    });
}

TEST(correctness, capacity_constructor) {
    faulty_run([] {
        counted::no_new_instances_guard guard;

        counted_buffer buffer(100);

        EXPECT_EQ(0, buffer.size());
        EXPECT_EQ(true, buffer.empty());
        EXPECT_EQ(100, buffer.capacity());

        guard.expect_no_instances();
    });
}

TEST(correctness, forward_iterator_constructor) {
    std::vector<counted> v = vec(100);

    faulty_run([&v] {
        counted_buffer buffer(v.begin(), v.end());

        EXPECT_EQ(v.size(), buffer.size());
        EXPECT_EQ(v.size(), buffer.capacity());
        EXPECT_FALSE(buffer.empty());
        EXPECT_EQ(v.back(), buffer.back());
        EXPECT_EQ(v.front(), buffer.front());
        EXPECT_TRUE(same(v, buffer));
    });
}

TEST(correctness, push_back) {
    std::vector<counted> v = vec(100), subvec;

    faulty_run([&] {
       counted_buffer buffer(25);

       for (size_t i = 0; i < v.size(); ++i) {
           buffer.push_back(v[i]);

           {
               fault_injection_disable fd;

               if (i < 25) {
                   subvec = std::vector<counted>(v.begin(), v.begin() + i + 1);
               } else {
                   subvec = std::vector<counted>(v.begin() + (i - 24), v.begin() + i + 1);
               }
           }

           EXPECT_EQ(buffer.size(), std::min(i + 1, size_t(25)));
           EXPECT_FALSE(buffer.empty());
           EXPECT_EQ(25, buffer.capacity());
           EXPECT_EQ(v[i], buffer.back());
           EXPECT_TRUE(same(subvec, buffer));
       }
    });
}

TEST(correctness, input_iterator_constructor) {
    std::vector<counted> v = vec(100);

    faulty_run([&v] {
        counted_buffer buffer(25, v.begin(), v.end());

        EXPECT_EQ(25, buffer.size());
        EXPECT_EQ(25, buffer.capacity());
        EXPECT_FALSE(buffer.empty());
        EXPECT_EQ(v[buffer.size() - 1], buffer.back());
        EXPECT_EQ(v.front(), buffer.front());
    });
}
