#include <list>
#include <thread>
#include <vector>
#include <algorithm>

#include "gtest/gtest.h"

#include "basic_circular_buffer.h"
#include "circular_buffer.h"
#include "counted.h"
#include "fault_injection.h"

typedef basic_circular_buffer<counted> counted_buffer;
typedef circular_buffer<counted> counted_th_buffer;

std::mutex cerr_mutex;

std::unique_lock<std::mutex> lock_cerr() {
    return std::unique_lock<std::mutex>(cerr_mutex);
}

template<typename T>
T gen(size_t size) {
    T ret;

    for (size_t i = 0; i < size; ++i)
        ret.emplace_back(i);

    return ret;
}

std::vector<counted> genvec(size_t size) {
    return gen<std::vector<counted>>(size);
}

void trace(std::ostream &) {
}

void itrace(std::ostream &) {
}

template<typename U, typename... Us>
void trace(std::ostream &os, U const &u, Us &&... us) {
    os << "{";
    for (auto const &v : u) {
        os << v;
        os << ", ";
    }
    os << "}" << std::endl;
    trace(os, std::forward<Us>(us)...);
}

template<typename U, typename... Us>
void itrace(std::ostream &os, U const &u, Us &&... us) {
    os << "{";
    for (auto const &v : u) {
        os << v;
        os << ", ";
    }
    os << "}" << std::endl;
    itrace(os, std::forward<Us>(us)...);
}

template <typename U>
void sync_trace(U const& u) {
    auto ulock = lock_cerr();
    std::cerr << u << std::endl;
}

template<typename U, typename V>
void EXPECT_SAME(U const &u, V const &v) {
#if 0
    trace(std::cerr, u, v);
#endif

    EXPECT_TRUE(std::equal(u.begin(), u.end(), v.begin(), v.end()));
}

template<typename C, typename F>
void test(size_t c_size, F &&f) {
    C vbase = gen<C>(c_size), v;
    counted_buffer buffer;

    size_t capacity_step = std::max(1ul, c_size / 10);
    auto it = vbase.end();

    for (size_t capacity = capacity_step; capacity <= c_size; capacity += capacity_step) {
        std::advance(it, -capacity_step);

        faulty_run([&] {
            {
                fault_injection_disable fd;

                v = C(it, vbase.end());
                buffer = counted_buffer(capacity, v.begin(), v.end());
            }

            f(v, buffer);
        });
    }
}

template<typename F>
void mthread(size_t th, F &&f) {
    std::vector<std::thread> ts;

    while (th--) {
        ts.emplace_back(f);
    }

    for (auto &t : ts) {
        t.join();
    }
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
    auto v = genvec(100);

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
    auto v = genvec(100);
    std::vector<counted> subvec;

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
    auto v = genvec(100);
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
    auto v = genvec(100);
    counted_buffer base_buffer(v.begin(), v.end());

    faulty_run([&base_buffer] {
        counted_buffer buffer(base_buffer);

        EXPECT_SAME(base_buffer, buffer);
    });
}

TEST(correctness, move_construction) {
    auto v = genvec(100);
    counted_buffer base_buffer(v.begin(), v.end());

    EXPECT_NO_THROW(counted_buffer(std::move(base_buffer)));
    EXPECT_SAME(std::vector<counted>(), base_buffer);
}

TEST(correctness, move_assignment) {
    auto v = genvec(100);
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
    std::vector<counted> v = gen<std::vector<counted>>(100);
    std::vector<counted> v1(v.begin(), v.begin() + 20);
    std::vector<counted> v2(v.begin(), v.begin() + 50);
    std::vector<counted> v3(v.begin(), v.begin() + 100);

    counted_buffer base_buffer(v.begin(), v.begin() + 50);

    faulty_run([&] {
        counted_buffer b1, b2, b3;
        {
            fault_injection_disable fd;

            b1 = counted_buffer(v.begin(), v.begin() + 20);
            b2 = counted_buffer(v.begin(), v.begin() + 50);
            b3 = counted_buffer(v.begin(), v.begin() + 100);

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
    auto v = genvec(100);
    counted_buffer buffer(v.begin(), v.end());

    faulty_run([&] {
        EXPECT_NO_THROW([&] {
            EXPECT_SAME(v, buffer);
        }());
    });
}

TEST(correctness, const_iterators) {
    auto v = genvec(100);
    counted_buffer buffer(v.begin(), v.end());

    faulty_run([&] {
        EXPECT_NO_THROW([&] {
            EXPECT_TRUE(std::equal(v.cbegin(), v.cend(), buffer.cbegin(), buffer.cend()));
        }());
    });
}

TEST(correctness, reverse_iterators) {
    auto v = genvec(100);counted_buffer buffer(v.begin(), v.end());faulty_run([&] { EXPECT_NO_THROW([&] { EXPECT_TRUE(std::equal(v.rbegin(), v.rend(), buffer.rbegin(), buffer.rend())); }()); });
}

TEST(correctness, const_reverse_iterators) {
    auto v = genvec(100);
    counted_buffer buffer(v.begin(), v.end());

    faulty_run([&] {
        EXPECT_NO_THROW([&] {
            EXPECT_TRUE(std::equal(v.rbegin(), v.rend(), buffer.rcbegin(), buffer.rcend()));
        }());
    });
}

TEST(correctness, pop_front) {
    test<std::list<counted>>(10, [&](std::list<counted> &l, counted_buffer &buffer) {
        EXPECT_NO_THROW([&] {
            while (!l.empty()) {
                l.pop_front();
                buffer.pop_front();

                EXPECT_SAME(l, buffer);
            }

            EXPECT_SAME(l, buffer);
        }());
    });
}

TEST(correctness, pop_back) {
    test<std::list<counted>>(10, [&](std::list<counted> &l, counted_buffer &buffer) {
        EXPECT_NO_THROW([&] {
            while (!l.empty()) {
                l.pop_back();
                buffer.pop_back();

                EXPECT_SAME(l, buffer);
            }

            EXPECT_SAME(l, buffer);
        }());
    });
}

TEST(correctness, resize) {
    test<std::list<counted>>(100, [&](std::list<counted> &l, counted_buffer &buffer) {
        buffer.resize(buffer.size() / 2);

        auto it = l.begin();
        std::advance(it, l.size() / 2);
        l = std::list<counted>(l.begin(), it);

        EXPECT_SAME(l, buffer);
    });
}

TEST(th_correctness, copy_construction) {
    auto v = genvec(100);
    counted_th_buffer base_buffer(v.begin(), v.end());

    mthread(4, [&] {
        counted_th_buffer buffer(base_buffer);

        EXPECT_SAME(base_buffer, buffer);
    });
}

TEST(th_correctness, copy_assignment) {
    auto v = genvec(100);
    std::vector<counted> v1 = std::vector<counted>(v.begin(), v.begin() + 50);

    counted_th_buffer base_buffer(v.begin(), v.end());

    mthread(4, [&] {
        counted_th_buffer b1;

        {
            b1 = counted_th_buffer(v1.begin(), v1.end());
            EXPECT_SAME(b1, v1);
        }

        b1 = base_buffer;
        EXPECT_SAME(base_buffer, b1);
    });
}

TEST(th_correctness, multiple_threads_push_back) {
    auto v = genvec(100);
    counted_th_buffer buffer(400);

    mthread(4, [&] {
        for (auto const &x : v) {
            buffer.push_back(x);
        }
    });

    EXPECT_EQ(400, buffer.size().first);

#if 0
    trace(std::cout, buffer);
#endif
}

std::thread producer(counted_th_buffer& buffer, size_t count) {
    return std::thread([&buffer, count] {
        for (size_t i = 0; i < count; ++i) {
            buffer.push_back(i);
        }
    });
}

template <typename F>
std::thread consumer(counted_th_buffer& buffer, F&& consume, size_t count) {
    return std::thread([&buffer, consume, count] {
        for (size_t i = 0; i < count; ++i) {
#if 0
            sync_trace(*consume(buffer));
#else
            consume(buffer);
#endif
        }
    });
}

template <typename F>
void consume_test(F&& consume, size_t consume_by_th) {
    std::vector<std::thread> ts;
    counted_th_buffer buffer(1000);

    for (size_t i = 0; i < 10; ++i) {
        ts.emplace_back(producer(buffer, 100));
    }

    for (size_t i = 0; i < 10; ++i) {
        ts.emplace_back(consumer(buffer, std::forward<F>(consume), consume_by_th));
    }

    for (auto& t : ts) {
        t.join();
    }

#if 0
    trace(std::cout, buffer);
#endif
}

TEST(th_correctness, wait_pop) {
    consume_test([] (counted_th_buffer& buffer) { return buffer.wait_pop(); }, 100);
}

TEST(th_correctness, try_pop) {
    consume_test([] (counted_th_buffer& buffer) { return buffer.try_pop(); }, 100);
}

TEST(th_correctness, wait_npop) {
    consume_test([] (counted_th_buffer& buffer) {
        auto v = genvec(100);
        return buffer.wait_npop(v.begin(), 100);
    }, 1);
}