#pragma once

#include <string>
#include <vector>
#include <memory>
#include <boost/optional.hpp>

#include "provider.hpp"

namespace libral {
  /* type is really a misnomer for this class; it's not really the type you
     are used from Puppet, since it's not concerned (yet?) with validation
     against a schema etc. Rather, it's mostly a very simple manager for
     providers */
  class type {
  public:
    type(const std::string name, std::shared_ptr<provider> prov)
      : _name(name), _prov(prov) { }
    const std::string& name(void) const { return _name; }
    result<boost::optional<resource>> find(const std::string &name);
    result<std::vector<resource>> instances(void);
    result<std::pair<update, changes>> set(const resource& should);
    void flush() { _prov->flush(); }

    /* Turn a string into a value. Return an error message if that is not
       possible */
    result<value> parse(const std::string &name, const std::string &v);

    const provider& prov() { return *_prov; };
  private:
    std::string _name;
    std::shared_ptr<provider> _prov;
  };
}
