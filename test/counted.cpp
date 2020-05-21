#include "counted.h"
#include "../gtest/gtest.h"

#include "fault_injection.h"

namespace {
int transcode(int data, void const* ptr) {
    return data ^ static_cast<int>(reinterpret_cast<std::ptrdiff_t>(ptr) / sizeof(counted));
}

std::mutex im;
}

counted::counted(int data)
    : data(transcode(data, this)) {
    std::lock_guard<std::mutex> lg(im);

    fault_injection_point();
    fault_injection_disable fd;
    auto p = instances.insert(this);
    EXPECT_TRUE(p.second);
}

counted::counted(counted const& other)
    : data(transcode(transcode(other.data, &other), this)) {
    std::lock_guard<std::mutex> lg(im);

    fault_injection_point();
    fault_injection_disable fd;
    auto p = instances.insert(this);
    EXPECT_TRUE(p.second);
}

counted::~counted() {
    std::lock_guard<std::mutex> lg(im);

    fault_injection_disable fd;
    size_t n = instances.erase(this);
    EXPECT_EQ(1u, n);
}

counted& counted::operator=(counted const& c) {
    std::lock_guard<std::mutex> lg(im);

    fault_injection_point();
    {
        fault_injection_disable fd;
        EXPECT_TRUE(instances.find(this) != instances.end());
    }

    data = transcode(transcode(c.data, &c), this);
    return *this;
}

counted::operator int() const {
    std::lock_guard<std::mutex> lg(im);

    fault_injection_disable fd;
    EXPECT_TRUE(instances.find(this) != instances.end());

    return transcode(data, this);
}

std::set<counted const*> counted::instances;

counted::no_new_instances_guard::no_new_instances_guard() {
    std::lock_guard<std::mutex> lg(im);

    old_instances = instances;
}

counted::no_new_instances_guard::~no_new_instances_guard() {
    std::lock_guard<std::mutex> lg(im);

    fault_injection_disable fd;
    EXPECT_TRUE(old_instances == instances);
}

void counted::no_new_instances_guard::expect_no_instances() {
    std::lock_guard<std::mutex> lg(im);

    fault_injection_disable fd;
    EXPECT_TRUE(old_instances == instances);
}