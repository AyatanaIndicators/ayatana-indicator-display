/*
 * Copyright 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#pragma once

#include <core/property.h>

#include <memory>
#include <string>

class BaseGreeter
{
public:
    BaseGreeter();
    virtual ~BaseGreeter();

    enum class State { UNAVAILABLE, INACTIVE, ACTIVE };
static inline const char* state_str(const State& state) {
    static constexpr char const * state_str[] = { "Unavailable", "Inactive", "Active" };
    return state_str[int(state)];
}
    virtual core::Property<State>& state() =0;
};


class Greeter: public BaseGreeter
{
public:
    Greeter();
    virtual ~Greeter();
    core::Property<State>& state() override;

protected:
    class Impl;
    std::unique_ptr<Impl> impl;
};

