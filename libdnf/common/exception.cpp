/*
Copyright Contributors to the libdnf project.

This file is part of libdnf: https://github.com/rpm-software-management/libdnf/

Libdnf is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

Libdnf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with libdnf.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "libdnf/common/exception.hpp"

#include "utils/bgettext/bgettext-lib.h"
#include "utils/bgettext/bgettext-mark-domain.h"

#include <algorithm>
#include <charconv>
#include <system_error>


namespace libdnf {


AssertionError::AssertionError(const char * assertion, const SourceLocation & location, const std::string & message)
    : logic_error(message),
      condition(assertion),
      location(location) {}


const char * AssertionError::what() const noexcept {
    try {
        str_what = location.file_name + std::string(":") + std::to_string(location.source_line) + ": " +
                   location.function_name;
        if (condition) {
            str_what += std::string(": Assertion '") + condition + "' failed: ";
        } else {
            str_what += ": Assertion failed: ";
        }
        str_what += logic_error::what();
        return str_what.c_str();
    } catch (...) {
        return logic_error::what();
    }
}

UserAssertionError::UserAssertionError(
    const char * assertion, const SourceLocation & location, const std::string & message)
    : logic_error(message),
      condition(assertion),
      location(location) {}


const char * UserAssertionError::what() const noexcept {
    try {
        str_what = location.file_name + std::string(":") + std::to_string(location.source_line) + ": " +
                   location.function_name;
        if (condition) {
            str_what += std::string(": API Assertion '") + condition + "' failed: ";
        } else {
            str_what += ": API Assertion failed: ";
        }
        str_what += logic_error::what();
        return str_what.c_str();
    } catch (...) {
        return logic_error::what();
    }
}

SystemError::SystemError(int error_code) : Error(M_("System error")), error_code(error_code), has_user_message(false) {}

std::string SystemError::get_error_message() const {
    return std::system_category().default_error_condition(error_code).message();
}


static std::string format(const std::exception & e, std::size_t level, bool with_domain) {
    std::string ret(level, ' ');
    if (auto ex = dynamic_cast<const Error *>(&e)) {
        if (with_domain) {
            ret += fmt::format("{}::{}: {}\n", ex->get_domain_name(), ex->get_name(), ex->what());
        } else {
            ret += fmt::format("{}: {}\n", ex->get_name(), ex->what());
        }
    } else {
        ret += e.what();
        ret += '\n';
    }
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception & e) {
        ret += format(e, level + 1, with_domain);
    } catch (...) {
    }

    return ret;
}

std::string format(const std::exception & e, bool with_domain) {
    return format(e, 0, with_domain);
}

Error::Error(const Error & e) noexcept : std::runtime_error(e) {
    operator=(e);
}


Error & Error::operator=(const Error & e) noexcept {
    this->std::runtime_error::operator=(e);

    try {
        message = e.message;
        format = e.format;
        formatter = e.formatter;
    } catch (...) {
    }

    return *this;
}


const char * Error::what() const noexcept {
    if (!formatter) {
        // formatter not set means copy constructor or assigment operator failed
        return TM_(format, 1);
    }

    try {
        message = formatter(TM_(format, 1));
        return message.c_str();
    } catch (...) {
        return std::runtime_error::what();
    }
}

const char * SystemError::what() const noexcept {
    std::string error_message;
    try {
        error_message = std::system_category().default_error_condition(error_code).message();
    } catch (...) {
    }

    if (has_user_message) {
        try {
            message = fmt::format(
                "{}: ({}) - {}", formatter ? formatter(TM_(format, 1)) : TM_(format, 1), error_code, error_message);
        } catch (...) {
            return TM_(format, 1);
        }
    } else {
        try {
            message = fmt::format("({}) - {}", error_code, error_message);
        } catch (...) {
            if (error_message.empty()) {
                return std::runtime_error::what();
            }
            message = std::move(error_message);
        }
    }
    return message.c_str();
}

const char * RuntimeError::get_description() const noexcept {
    return _("General RuntimeError exception");
}

}  // namespace libdnf
