#ifndef VAST_KEY_H
#define VAST_KEY_H

#include <vector>
#include "vast/string.h"
#include "vast/util/print.h"
#include "vast/util/parse.h"

namespace vast {

/// A sequence of type/argument names to recursively access a type or value.
struct key : std::vector<string>,
             util::printable<key>,
             util::parsable<key>
{
  using super = std::vector<string>;

  key() = default;

  key(super::const_iterator begin, super::const_iterator end)
    : super{begin, end}
  {
  }

  key(super v)
    : super{std::move(v)}
  {
  }

  key(std::initializer_list<string> list)
    : super{std::move(list)}
  {
  }

  template <typename Iterator>
  bool print(Iterator& out) const
  {
    auto f = begin();
    auto l = end();
    while (f != l)
      if (render(out, *f) && ++f != l && ! render(out, "."))
        return false;

    return true;
  }

  template <typename Iterator>
  bool parse(Iterator& begin, Iterator end)
  {
    string str;
    if (! extract(begin, end, str))
      return false;

    auto s = str.split(".");
    if (s.empty())
      return false;

    for (auto& p : s)
      emplace_back(p.first, p.second);

    return true;
  }
};

} // namespace vast

#endif