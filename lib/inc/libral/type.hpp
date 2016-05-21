#pragma once

#include <string>
#include <vector>
#include <memory>

#include "provider.hpp"

namespace libral {
  class type {
  public:
    type(const std::string name) : _name(name) { }
    const std::string& name(void) const { return _name; }
    virtual std::vector<std::unique_ptr<resource>> instances(void);
    virtual std::string id() { return "type/" + name(); }
    resource instance(const std::string& name);
  private:
    std::string _name;
  };

  class mount_type : public type {
  public:
    mount_type() : type("mount") { }
    std::vector<std::unique_ptr<resource>> instances(void);
    std::string id() { return "mount_type"; }
  };
}