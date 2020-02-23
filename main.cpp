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
void EXPECT_SAME(U const& u, V const& v) {
    EXPECT_EQ(u.size(), v.size());

    for (size_t i = 0; i < u.size(); ++i)
        EXPECT_EQ(u[i], v[i]);
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
        EXPECT_SAME(v, buffer);
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
           EXPECT_SAME(subvec, buffer);
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

TEST(correctness, copy_construction) {
    std::vector<counted> v = vec(100);
    counted_buffer base_buffer(v.begin(), v.end());

    faulty_run([&base_buffer] {
        counted_buffer buffer(base_buffer);

        EXPECT_SAME(base_buffer, buffer);
    });
}

TEST(correctness, move_construction) {
    std::vector<counted> v = vec(100);
    counted_buffer base_buffer(v.begin(), v.end());

    EXPECT_NO_THROW(counted_buffer(std::move(base_buffer)));
    EXPECT_SAME(std::vector<counted>(), base_buffer);
}

TEST(correctness, move_assignment) {
    std::vector<counted> v = vec(100);
    std::vector<counted> v1(v.begin(), v.begin() + 20);
    std::vector<counted> v2(v.begin(), v.begin() + 60);
    std::vector<counted> v3(v.begin(), v.begin() + 100);
    counted_buffer b1, b2, b3;

    EXPECT_NO_THROW(b1 = counted_buffer(v.begin(), v.begin() + 20));
    EXPECT_NO_THROW(b2 = counted_buffer(v.begin(), v.begin() + 60));
    EXPECT_NO_THROW(b3 = counted_buffer(v.begin(), v.begin() + 100));

    EXPECT_SAME(b1, v1);
    EXPECT_SAME(b2, v2);
    EXPECT_SAME(b3, v3);
}

TEST(correctness, copy_assignment) {
    std::vector<counted> v = vec(20);
    std::vector<counted> v1(v.begin(), v.begin() + 2);
    std::vector<counted> v2(v.begin(), v.begin() + 6);
    std::vector<counted> v3(v.begin(), v.begin() + 15);

    counted_buffer base_buffer(v.begin(), v.begin() + 10);

    faulty_run([&] {
        counted_buffer b1, b2, b3;
        {
            fault_injection_disable fd;

            b1 = counted_buffer(v.begin(), v.begin() + 2);
            b2 = counted_buffer(v.begin(), v.begin() + 6);
            b3 = counted_buffer(v.begin(), v.begin() + 15);

            EXPECT_SAME(b1, v1);
            EXPECT_SAME(b2, v2);
            EXPECT_SAME(b3, v3);
        }

        b1 = base_buffer;
        b2 = base_buffer;
        b3 = base_buffer;

        EXPECT_SAME(base_buffer, b1);
        EXPECT_SAME(base_buffer, b2);
        EXPECT_SAME(base_buffer, b3);
    });
}

TEST(correctness, iterators) {
    std::vector<counted> v = vec(100);
    counted_buffer buffer(v.begin(), v.end());

    faulty_run([&] {
        EXPECT_NO_THROW([&] {
            EXPECT_TRUE(std::equal(v.begin(), v.end(), buffer.begin(), buffer.end()));
        }());
    });
}

TEST(correctness, const_iterators) {
    std::vector<counted> v = vec(100);
    counted_buffer buffer(v.begin(), v.end());

    faulty_run([&] {
        EXPECT_NO_THROW([&] {
            EXPECT_TRUE(std::equal(v.begin(), v.end(), buffer.cbegin(), buffer.cend()));
        }());
    });
}

TEST(correctness, reverse_iterators) {
    std::vector<counted> v = vec(100);
    counted_buffer buffer(v.begin(), v.end());

    faulty_run([&] {
        EXPECT_NO_THROW([&] {
            EXPECT_TRUE(std::equal(v.rbegin(), v.rend(), buffer.rbegin(), buffer.rend()));
        }());
    });
}

TEST(correctness, const_reverse_iterators) {
    std::vector<counted> v = vec(100);
    counted_buffer buffer(v.begin(), v.end());

    faulty_run([&] {
        EXPECT_NO_THROW([&] {
            EXPECT_TRUE(std::equal(v.rbegin(), v.rend(), buffer.rcbegin(), buffer.rcend()));
        }());
    });
}