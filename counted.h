#ifndef COUNTED_H
#define COUNTED_H

#include <set>

struct counted {
    struct no_new_instances_guard;

    counted() = delete;
    counted(int data);
    counted(counted const& other);
    ~counted();

    counted& operator=(counted const& c);
    operator int() const;

private:
    int data;

    static std::set<counted const*> instances;
};

struct counted::no_new_instances_guard {
    no_new_instances_guard();

    no_new_instances_guard(no_new_instances_guard const&) = delete;
    no_new_instances_guard& operator=(no_new_instances_guard const&) = delete;

    ~no_new_instances_guard();

    void expect_no_instances();

private:
    std::set<counted const*> old_instances;
};

#endif // COUNTED_H
