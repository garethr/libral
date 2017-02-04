#include <libral/user.hpp>

#include <leatherman/locale/locale.hpp>

#include <sys/types.h>
#include <pwd.h>

using namespace leatherman::locale;

namespace libral {
  result<prov::spec> user_provider::describe() {
    static const std::string desc =
#include "user.yaml"
      ;
    return prov::spec::read("user", desc);
  }

  result<bool> user_provider::suitable() {
    _cmd_useradd = command::create("useradd");
    _cmd_usermod = command::create("usermod");
    _cmd_userdel = command::create("userdel");
    return _cmd_useradd && _cmd_usermod && _cmd_userdel;;
  }

  void user_provider::flush() {
    return; // Noop, no batching
  }

  std::unique_ptr<resource> user_provider::create(const std::string& name) {
    auto shared_this = std::static_pointer_cast<user_provider>(shared_from_this());
    return std::unique_ptr<resource>(new user_resource(shared_this, name, false));
  }

  result<std::vector<resource_uptr>> user_provider::instances() {
    std::vector<std::unique_ptr<resource>> result;
    auto shared_this = std::static_pointer_cast<user_provider>(shared_from_this());
    struct passwd *p = NULL;

    setpwent();
    while ((p = getpwent()) != NULL) {
      // FIXME: check errno according to getpwent(3)
      auto res = std::unique_ptr<resource>(new user_resource(shared_this, p->pw_name, true));
      (*res)["ensure"]  = "present";
      (*res)["comment"] = std::string(p->pw_gecos);
      (*res)["gid"]     = std::to_string(p->pw_gid);
      (*res)["home"]    = std::string(p->pw_dir);
      (*res)["shell"]   = std::string(p->pw_shell);
      (*res)["uid"]     = std::to_string(p->pw_uid);
      result.push_back(std::move(res));
    }
    endpwent();

    return std::move(result);
  }

  result<changes>
  user_provider::user_resource::update(const attr_map& should) {
    auto props = { "comment", "gid", "home", "shell", "uid" };

    auto& self = *this;
    auto res = result<changes>(changes());
    changes& chgs = res.ok();

    auto state = lookup<std::string>("ensure", "absent");
    auto ensure = should.lookup<std::string>("ensure", state);

    if (ensure != state) {
      chgs.push_back(change("ensure", ensure, state));
    }
    self["ensure"] = ensure;

    if (ensure == "present") {
      for (auto prop : props) {
        auto p = should.lookup<std::string>(prop);
        if (p && self[prop] != value(*p)) {
          chgs.push_back(change(prop, *p, self[prop]));
          self[prop] = *p;
        }
      }

      if (!_exists || !chgs.empty()) {
        std::vector<std::string> args;
        if (self["comment"].is_present()) {
          args.push_back("-c");
          args.push_back(self["comment"].to_string());
        }
        if (self["gid"].is_present()) {
          args.push_back("-g");
          args.push_back(self["gid"].to_string());
        }
        if (self["home"].is_present()) {
          args.push_back("-d");
          args.push_back(self["home"].to_string());
        }
        if (self["shell"].is_present()) {
          args.push_back("-s");
          args.push_back(self["shell"].to_string());
        }
        if (self["uid"].is_present()) {
          args.push_back("-u");
          args.push_back(self["uid"].to_string());
        }
        args.push_back(name());
        result<bool> run_result;
        if (_exists) {
          run_result = _prov->_cmd_usermod->run(args);
        } else {
          run_result = _prov->_cmd_useradd->run(args);
        }
        if (! run_result)
          return run_result.err();
      }
    } else if (ensure == "absent") {
      auto run_result = _prov->_cmd_userdel->run({ "-r", name() });
      if (! run_result)
        return run_result.err();
    } else if (ensure == "role") {
      return error("can not ensure=role with this provider");
    }
    return res;
  }
}
