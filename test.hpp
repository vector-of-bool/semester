/**
 * This file is GENERATED! DO NOT EDIT!
 */
#pragma once

#include <semester/json.hpp>

#include <optional>
#include <variant>

namespace dds {

class _transform_type {};

class _git_type {
    std::optional<std::string>   auto_lib;
    std::string                  ref;
    std::string                  url;
    std::vector<_transform_type> transform;
};

class _packages_type {
    std::map<std::string, std::string> depends;
    std::optional<std::string>         description;
    std::optional<_git_type>           git;
};

class catalog {
    double                                version;
    std::map<std::string, _packages_type> packages;
};

}  // namespace dds
