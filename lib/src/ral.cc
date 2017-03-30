#include <libral/ral.hpp>

#include <config.hpp>

#include <libral/mount.hpp>
#include <libral/user.hpp>
#include <libral/file.hpp>
#include <libral/type.hpp>
#include <libral/provider.hpp>

#include <libral/simple_provider.hpp>
#include <libral/json_provider.hpp>
#include <leatherman/file_util/directory.hpp>
#include <leatherman/execution/execution.hpp>
#include <leatherman/logging/logging.hpp>

#include <boost/nowide/iostream.hpp>

#include <yaml-cpp/yaml.h>

namespace exe = leatherman::execution;
using namespace leatherman::locale;

namespace libral {
  std::shared_ptr<ral> ral::create(const std::vector<std::string>& data_dirs) {
    auto handle = new ral(data_dirs);

    return std::shared_ptr<ral>(handle);
  }

  ral::ral(const std::vector<std::string>& data_dirs) : _data_dirs(data_dirs) { }

  bool ral::add_type(std::vector<std::unique_ptr<type>>& types,
                     environment &env,
                     const std::string& name, std::shared_ptr<provider> prov) {
    auto res = prov->prepare(env);
    if (res.is_ok()) {
      if (prov->spec()->suitable()) {
        auto t = new type(prov);
        types.push_back(std::unique_ptr<type>(t));
        return true;
      } else {
        LOG_INFO("provider[{1}] for {2} is not suitable", prov->source(), name);
      }
    } else {
      LOG_WARNING("provider[{1}] failed to prepare: {2}", name, res.err().detail);
    }
    return false;
  }

  std::vector<std::unique_ptr<type>> ral::types(void) {
    // @todo lutter 2016-05-10:
    //   Need more magic here: need to find and register all types
    std::vector<std::unique_ptr<type>> result;
    environment env = make_env();

    auto mount_prov = std::shared_ptr<provider>(new mount_provider());
    add_type(result, env, "mount", mount_prov);

    auto user_prov = std::shared_ptr<provider>(new user_provider());
    add_type(result, env, "user", user_prov);

    auto file_prov = std::shared_ptr<provider>(new file_provider());
    add_type(result, env, "file", file_prov);

    // Find external providers
    auto cb = [&result,&env,this](std::string const &path) {
      auto res = run_describe(path);
      if (! res) {
        LOG_WARNING("provider[{1}]: {2}", path, res.err().detail);
        return true;
      }

      auto parsed = parse_metadata(path, *res);
      if (!parsed) {
        LOG_ERROR("provider[{1}]: {2}", path, parsed.err().detail);
        return true;
      }

      YAML::Node node = *parsed;
      auto meta = node["provider"];
      auto type_name = meta["type"].as<std::string>("(none)");
      if (type_name == "(none)") {
        LOG_ERROR("provider[{1}]: missing a type name under 'provider.type'",
                  path);
        return true;
      }

      auto invoke = meta["invoke"].as<std::string>("(none)");
      if (invoke == "simple" || invoke == "json") {
        provider *raw_prov;
        if (invoke == "simple") {
          raw_prov = new simple_provider(path, node);
        } else {
          raw_prov = new json_provider(path, node);
        }
        auto prov = std::shared_ptr<provider>(raw_prov);
        if (add_type(result, env, type_name, prov)) {
          LOG_INFO(_("provider[{1}] ({2}) for {3} loaded",
                     path, invoke, type_name));
        }
      } else if (invoke == "(none)") {
        LOG_ERROR("provider[{1}]: no calling convention given under 'provider.invoke'",
                  path);
      } else {
        LOG_ERROR("provider[{1}]: unknown calling convention '{2}'",
                  path, invoke);
      }
      return true;
    };

    for (auto dir : _data_dirs) {
      leatherman::file_util::each_file(dir + "/providers", cb , "\\.prov$");
    }

    std::sort(result.begin(), result.end(),
              [](const std::unique_ptr<type>& a,
                 const std::unique_ptr<type>& b)
              { return a->prov().spec()->qname() < b->prov().spec()->qname(); });

    return result;
  }

  boost::optional<std::unique_ptr<type>>
  ral::find_type(const std::string& name) {
    // FIXME: Using unique_ptr here is pretty dumb, as it only works
    // because we populate types() every time it is used
    auto types_vec = types();


    for (auto& t : types_vec) {
      // FIXME: We assume that qname is unique amongst all providers, but
      // do not enforce that
      if (t->qname() == name) {
        return std::move(t);
      }
    }

    std::unique_ptr<type> typ;
    for (auto& t : types_vec) {
      if (t->type_name() == name) {
        if (!typ) {
          typ = std::move(t);
        } else {
          // At least two providers with the same name
          return boost::none;
        }
      }
    }
    if (typ)
      return std::move(typ);
    else
      return boost::none;
  }

  result<YAML::Node>
  ral::parse_metadata(const std::string& path, const std::string& yaml) const {
    YAML::Node node;
    try {
      node = YAML::Load(yaml);
    } catch (YAML::Exception& e) {
      return error(_("metadata is not valid yaml: {1}", e.what()));
    }
    if (!node) {
      return error(_("metadata is not valid yaml"));
    }
    if (!node.IsMap()) {
      return error(_("metadata must be a yaml map"));
    }
    auto meta = node["provider"];
    if (!meta || !meta.IsMap()) {
      return error(_("metadata must have toplevel 'provider' key containing a map"));
    }
    return node;
  }

  result<std::string> ral::run_describe(const std::string& path) const {
    if (access(path.c_str(), X_OK) == 0) {
      auto res = exe::execute(path, { "ral_action=describe" },
                              0, { exe::execution_options::trim_output,
                                  exe::execution_options::merge_environment });
      if (!res.success) {
        if (res.output.empty()) {
          return error(_("ignored as it exited with status {1}", res.exit_code));
        } else {
          return error(_("ignored as it exited with status {1}. Output was '{2}'",
                         res.exit_code, res.output));
        }
      }
      if (! res.error.empty()) {
        return error(_("ignored as it produced stderr '{1}'", res.error));
      }
      return res.output;
    }
    // We only get here if path is not executable
    return error(_("file {1} looks like a provider but is not executable",
                   path));
  }
}
