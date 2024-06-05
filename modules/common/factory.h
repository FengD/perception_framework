// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: factory tamplate

#pragma once

#include <glog/logging.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/algorithm/string/case_conv.hpp>

namespace crdc {
namespace airi {
static inline std::string to_upper(const std::string& a) {
  return boost::to_upper_copy<std::string>(a);
}

template <typename E>
struct ComponentTraits {
 public:
  static std::string name() {return "";}
};

namespace traits {

template <typename T, typename... Args>
struct has_void_init {
  template <typename U, void (U::*)(Args...)>
  struct SFINAE {};
  template <typename U>
  static char Test(SFINAE<U, &U::init>*);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);
};

template <typename T, typename... Args>
struct has_boolean_init {
  template <typename U, void (U::*)(Args...)>
  struct SFINAE {};
  template <typename U>
  static char Test(SFINAE<U, &U::init>*);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);
};
}  // namespace traits

template <typename E>
class ComponentFactory {
 public:
  typedef std::shared_ptr<E> ElePtr;
  typedef ElePtr (*Creator)();
  typedef std::map<std::string, Creator> Registry;

  struct SingletonRegistry {
    std::mutex mutex;
    std::unordered_map<std::string, ElePtr> registry;
  };

  static Registry& get_registry() {
    static std::map<std::string, std::shared_ptr<Registry>> registry_map_;
    auto ele = ComponentTraits<E>::name();
    auto search = registry_map_.find(ele);
    if (search == registry_map_.end()) {
      registry_map_[ele] = std::shared_ptr<Registry>(new Registry());
    }
    return *registry_map_[ele];
  }

  static void register_creator(const std::string& type, Creator c) {
    auto& registry = get_registry();
    std::string upper = to_upper(type);
    auto search = registry.find(upper);
    CHECK(search == registry.end()) << upper << " has been registered!";
    registry[upper] = c;
  }

  static ElePtr get(const std::string& type) {
    auto& registry = get_registry();
    std::string upper = to_upper(type);
    auto search = registry.find(upper);
    CHECK(search != registry.end()) << ComponentTraits<E>::name() << ": "
                                    << upper << " not registered!";
    return registry[upper]();
  }

  static ElePtr get_singleton(const std::string& name, const std::string& type = "") {
    auto& reg = singleton_registry();
    std::string upper_name = to_upper(name);
    std::lock_guard<std::mutex> lock(reg.mutex);
    auto& registry = reg.registry;
    if (registry.find(upper_name) == registry.end()) {
      CHECK(!type.empty()) << "Empty type when create singleton instance";
      std::string upper_type = to_upper(type);
      auto inst = get(upper_type);
      registry[upper_name] = inst;
    }
    return registry.at(upper_name);
  }

  template <typename... Args>
  static ElePtr get_inited_singleton(const std::string& type, const std::string& name,
                                     Args... params) {
    static std::mutex singleton_lock_;
    static std::unordered_map<std::string, std::unordered_map<std::string, ElePtr>> inst_registry;
    std::string upper_type = to_upper(type);
    std::string upper_name = to_upper(name);
    std::lock_guard<std::mutex> lock(singleton_lock_);
    if (inst_registry.find(upper_type) == inst_registry.end() ||
        inst_registry.at(upper_type).find(upper_name) == inst_registry.at(upper_type).end()) {
      auto inst = get(upper_type);
      init_inst(inst, std::integral_constant<bool, traits::has_void_init<E, Args...>::value>(),
                std::integral_constant<bool, traits::has_boolean_init<E, Args...>::value>(),
                params...);
      inst_registry[upper_type][upper_name] = inst;
    }
    return inst_registry.at(upper_type).at(upper_name);
  }

  static std::vector<std::string> available() {
    auto& registry = get_registry();
    std::vector<std::string> ret;
    for (auto& r : registry) {
      ret.emplace_back(r.first);
    }
    return ret;
  }

 private:
  ComponentFactory() {}

  static SingletonRegistry& singleton_registry() {
    static SingletonRegistry inst_registry;
    return inst_registry;
  }

  template <typename... Args>
  static void init_inst(ElePtr& inst, std::true_type, std::false_type, Args... params) {
    inst->init(params...);
  }

  template <typename... Args>
  static void init_inst(ElePtr& inst, std::true_type, std::true_type, Args... params) {
    CHECK(inst->init(params...)) << "Failed to init singleton for [" << ComponentTraits<E>::name()
                                 << "]";
  }

  template <typename... Args>
  static void init_inst(ElePtr& inst, std::false_type, std::true_type, Args... params) {
    CHECK(inst->init(params...)) << "Failed to init singleton for [" << ComponentTraits<E>::name()
                                 << "]";
  }

  template <typename...>
  static void init_inst(ElePtr& inst, std::false_type, std::false_type, ...) {
    LOG(INFO) << "[" << ComponentTraits<E>::name() << "] has no init(...), skip";
  }
};

template <typename E>
class ComponentRegisterer {
 public:
  typedef std::shared_ptr<E> ElePtr;
  typedef ElePtr (*Creator)();
  ComponentRegisterer(const std::string& type, Creator c) {
    ComponentFactory<E>::register_creator(type, c);
  }
};

#define REGISTER_COMPONENT(com)                                    \
  template <>                                                      \
  struct ComponentTraits<com> {                                    \
    static std::string name() { return #com; }                     \
  };                                                               \
  using com##Factory = crdc::airi::ComponentFactory<com>;          \
  using com##Registerer = crdc::airi::ComponentRegisterer<com>
}  // namespace airi
}  // namespace crdc

#define REGISTER_CLASS(com, type)                                                            \
  std::shared_ptr<com> create_##com##_##type() { return std::shared_ptr<com>(new type()); }  \
  static com##Registerer g_registerer_##com##type(#type, create_##com##_##type)
