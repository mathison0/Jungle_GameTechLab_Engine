#pragma once

#include <memory>
#include <stack>

template <typename T>
using TStack = std::stack<T, std::vector<T>>;
