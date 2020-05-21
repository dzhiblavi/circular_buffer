#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#include <functional>

struct injected_fault : std::runtime_error {
    using runtime_error::runtime_error;
};

struct fault_injection_disable {
    fault_injection_disable();

    fault_injection_disable(fault_injection_disable const&) = delete;

    fault_injection_disable& operator=(fault_injection_disable const&) = delete;

    ~fault_injection_disable();

private:
    bool was_disabled;
};

void faulty_run(std::function<void()> const&);

bool should_inject_fault();

void fault_injection_point();

#endif // FAULT_INJECTION_H
