
#pragma once

#include <src/greeter.h>

inline void PrintTo(const Greeter::State& state, std::ostream* os) {
    switch(state) {
        case Greeter::State::ACTIVE:      *os << "Active"; break;
        case Greeter::State::INACTIVE:    *os << "Inactive"; break;
        case Greeter::State::UNAVAILABLE: *os << "Unavailable"; break;
    }
}

inline std::ostream& operator<<(std::ostream& os, const Greeter::State& state) {
    PrintTo(state, &os);
    return os;
}

