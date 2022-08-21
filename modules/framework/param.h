#pragma once

#include <glog/logging.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <boost/any.hpp>
#include "framework/proto/type.pb.h"

namespace crdc {
namespace airi {

class Param {
 public:
  Param() = default;
  virtual ~Param() = default;
  template <typename T>
  Param(const std::string& name, T value) : name_(name) {
    param_ = value;
  }
  template <typename T>
  bool get(T* value) const {
    if (typeid(T) != param_.type()) {
      return false;
    }
    *value = boost::any_cast<T>(param_);
    return true;
  }
  template <typename T>
  bool set(const T& value) {
    if (typeid(T) != param_.type()) {
      return false;
    }
    param_ = value;
    return true;
  }
  std::string name() const { return name_; }

 private:
  std::string name_;
  boost::any param_;
};

class ParamManager {
 public:
  typedef ::google::protobuf::RepeatedPtrField<::crdc::airi::AnyParam> PBParamType;
  bool init_params(const PBParamType& params) {
    for (int i = 0; i < params.size(); ++i) {
      const auto param = params.Get(i);
      auto name = param.name();
      if (param.has_i()) {
        int p = param.i();
        add_param(Param(name, p));
      } else if (param.has_b()) {
        bool p = param.b();
        add_param(Param(name, p));
      } else if (param.has_f()) {
        float p = param.f();
        add_param(Param(name, p));
      } else if (param.has_s()) {
        std::string p = param.s();
        add_param(Param(name, p));
      } else {
        LOG(ERROR) << " param[" << param.name() << "] has no value";
        return false;
      }
    }
    return true;
  }
  const std::vector<Param>& params() const {
    std::unique_lock<std::mutex> lock(lock_);
    return params_;
  }
  void add_param(const Param& param) {
    std::unique_lock<std::mutex> lock(lock_);
    params_.emplace_back(param);
    params_map_[param.name()].emplace_back(param);
  }
  template <typename T>
  bool get_param(const int idx, std::string name, T* value) const {
    std::unique_lock<std::mutex> lock(lock_);
    if (params_map_.find(name) == params_map_.end()) {
      LOG(INFO) << "Param: no such key:" << name;
      return false;
    }
    if (idx >= static_cast<int>((params_map_.at(name).size()))) {
      LOG(ERROR) << "Param: required index > maximum: " << idx << " vs. "
             << params_map_.at(name).size();
      return false;
    }
    return params_map_.at(name).at(idx).get(value);
  }
  template <typename T>
  bool get_param(std::string& name, T* value) const {
    return get_param(0, name, value);
  }
  template <typename T>
  bool set_param(const size_t idx, std::string name, const T& value) {
    std::unique_lock<std::mutex> lock(lock_);
    if (params_map_.find(name) == params_map_.end()) {
      LOG(INFO) << "Param: no such key:" << name;
      return false;
    }
    if (idx >= params_map_.at(name).size()) {
      LOG(ERROR) << "Param: required index > maximum: " << idx << " vs. "
             << params_map_.at(name).size();
      return false;
    }
    return params_map_[name][idx].set(value);
  }
  template <typename T>
  bool set_param(std::string& name, const T& value) {
    return set_param(0, name, value);
  }
  size_t param_count(const std::string& name) const {
    std::unique_lock<std::mutex> lock(lock_);
    if (params_map_.find(name) == params_map_.end()) {
      return 0;
    }
    return params_map_.at(name).size();
  }

 protected:
  mutable std::mutex lock_;
  std::vector<Param> params_;
  std::unordered_map<std::string, std::vector<Param>> params_map_;
};
}  // namespace airi
}  // namespace crdc
