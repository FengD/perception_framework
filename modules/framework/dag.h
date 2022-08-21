// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: dag, this class is used to sort the dag operator

#pragma once

#include <algorithm>
#include <queue>
#include <vector>
#include <unordered_map>
#include <set>
#include <string>

namespace crdc {
namespace airi {
class DAG {
 public:
  /**
   * @brief this function is used to sort the dag file by input and output.
   *        And verify if the loop exist.
   * @param the links of the dag node
   * @param the output of the sorted node
   * @return weather the sort finished
   */
  static bool sort_and_check_has_loop(const std::vector<std::vector<int>>& links,
                                      std::vector<int>* sorted) {
    int n = links.size();
    std::queue<int> q;
    std::vector<int> indegree(n, 0);
    for (int i = 0; i < n; ++i) {
      for (auto& l : links[i]) {
        ++indegree[l];
      }
    }
    for (int i = 0; i < n; ++i) {
      if (indegree[i] == 0) {
        q.push(i);
      }
    }

    int count = 0;
    while (!q.empty()) {
      int v = q.front();
      q.pop();

      sorted->emplace_back(v);
      ++count;
      for (auto& l : links[v]) {
        --indegree[l];
        if (indegree[l] == 0) {
          q.push(l);
        }
      }
    }
    return (count == n);
  }

  /**
   * @brief Check all the operator config in the dag file and add the needed.
   * @param[in] dag prototext
   * @param[out] the list of the operator config[std::vector]
   * @return is the check success[bool]
   */
  static bool check_operator_config(const DAGConfig& dag_config,
                                    std::vector<OperatorConfig>& ops) {
    for (int i = 0; i < dag_config.op_size(); ++i) {
      auto& op = dag_config.op(i);
      if (op.has_enable_if() && op.has_disable_if()) {
        LOG(ERROR) << op.name() << " specified both `enable_if` and `disable_if`";
        return false;
      }
      auto o = op;
      if (!o.has_bypass()) {
        o.set_bypass(false);
        if (o.has_bypass_if()) {
          const char* env = ::getenv(op.bypass_if().c_str());
          if (env != NULL) {
            o.set_bypass(true);
            LOG(INFO) << o.name() << " bypassed by env: ${" << env << "}";
          }
        }
      }
      if (o.has_enable_if()) {
        const char* env = ::getenv(op.enable_if().c_str());
        if (env != NULL) {
          ops.emplace_back(o);
        } else {
          LOG(INFO) << o.name() << " disabled";
        }
      } else if (o.has_disable_if()) {
        const char* env = ::getenv(o.disable_if().c_str());
        if (env != NULL) {
          LOG(INFO) << o.name() << " disabled";
        } else {
          ops.emplace_back(o);
        }
      } else {
        ops.emplace_back(o);
      }
    }
    return true;
  }

  /**
   * @brief link and sort operators
   * @param[in] dag proto
   * @param[out] the sorted operator config lists
   * @return is sort successed[bool]
   */
  static bool sort_operator(const DAGConfig& dag_config,
                            std::vector<OperatorConfig>* ret) {
    std::unordered_map<std::string, std::set<int>> input_op_map;
    std::unordered_map<std::string, std::set<int>> trigger_op_map;
    std::vector<OperatorConfig> ops;
    if (!check_operator_config(dag_config, ops)) {
      LOG(ERROR) << "Failed to check operator config!";
      return false;
    }

    // init trigger map and input map
    for (size_t i = 0; i < ops.size(); ++i) {
      auto& op = ops[i];
      for (int j = 0; j < op.trigger_size(); ++j) {
        trigger_op_map[op.trigger(j)].insert(i);
      }
      for (int j = 0; j < op.input_size(); ++j) {
        input_op_map[op.input(j)].insert(i);
      }
    }

    // init operator links
    std::vector<std::set<int>> op_links(ops.size());
    for (size_t i = 0; i < ops.size(); ++i) {
      auto& op = ops[i];
      for (int j = 0; j < op.output_size(); ++j) {
        auto output_event = op.output(j).event();
        if (trigger_op_map.find(output_event) != trigger_op_map.end()) {
          auto in_idx = trigger_op_map.at(output_event);
          op_links[i].insert(in_idx.begin(), in_idx.end());
        }
        if (input_op_map.find(output_event) != input_op_map.end()) {
          auto in_idx = input_op_map.at(output_event);
          op_links[i].insert(in_idx.begin(), in_idx.end());
        }
      }
    }

    // adjust the streaming of all the operators
    std::vector<std::vector<int>> op_adj(ops.size());
    for (size_t i = 0; i < ops.size(); ++i) {
      auto it = op_links[i].find(i);
      if (it != op_links[i].end()) {
        op_links[i].erase(it);
      }
      std::copy(op_links[i].begin(), op_links[i].end(), std::back_inserter(op_adj[i]));
      auto& op = ops[i];
      LOG(INFO) << "DAG: " << op.name() << " Downstreams:";
      for (size_t j = 0; j < op_adj[i].size(); ++j) {
        LOG(INFO) << "  * " << ops[op_adj[i][j]].name();
      }
    }
    std::vector<int> ops_idx;
    if (!sort_and_check_has_loop(op_adj, &ops_idx)) {
      LOG(ERROR) << "DAG: Loop Detected";
      return false;
    }

    // sort successed. Rewrite the operator list
    ret->clear();
    for (size_t op_id = 0; op_id < ops_idx.size(); ++op_id) {
      auto& op = ops[ops_idx[op_id]];
      LOG(INFO) << "Operator[" << ops_idx[op_id] << "]: " << op.name();
      op.set_id(op_id);
      ret->emplace_back(op);
    }

    return true;
  }

  /**
   * @brief Get the data and the type name.
   *        Because the data type could heritate so this function is used
   *        to get the data and type name if it is not given in the dag config. 
   * @param[in]
   * @param[in]
   * @param[in]
   * @param[in]
   * @param[out]
   * @param[out]
   * @return
   */
  static bool get_data_and_type_name(const OperatorOutput& up_output,
                                    const OperatorOutput& down_output,
                                    const std::string& down_name,
                                    const std::string& down_trigger_name,
                                    std::string& data_name, std::string& type_name) {
    if (down_output.has_type() && down_output.has_hz()) {
      LOG(ERROR) << down_output.event() << " specified both output.type and output.hz";
      return false;
    }

    if (up_output.has_data()) {
      data_name = up_output.data();
    }
    if (down_output.has_hz()) {
      if (!down_output.has_data()) {
        data_name = down_trigger_name + "_CACHED_DATA_@" + std::to_string(down_output.hz());
      }
    }
    if (!down_output.has_type()) {
      if (!up_output.has_type() || up_output.type() == "") {
        LOG(ERROR) << "DAG: cannot infer Operator[" << down_name << "]"
                << " event<" << down_output.event() << ">'s type";
        return false;
      }
      type_name = up_output.type();
    } else {
      if (data_name.empty()) {
        data_name = down_output.type() + "_DATA";
      }
      type_name = down_output.type();
    }
    if (data_name.empty()) {
      if (!up_output.has_data() || up_output.data() == "") {
        LOG(ERROR) << "DAG: cannot infer Operator[" << down_name << "]"
                << " event<" << down_output.event() << ">'s data";
        return false;
      }
      data_name = up_output.data();
    }
    return true;
  }

  /**
   * @brief link the operator in the operator list
   * @param[in] the list of operator config
   * @return if the link successed[bool]
   */
  static bool link_operator(std::vector<OperatorConfig>* ops) {
    for (size_t i = 0; i < ops->size(); ++i) {
      auto& op = ops->at(i);
      for (int j = 0; j < op.output_size(); ++j) {
        op.mutable_output(j)->clear_downstream();
      }
    }
    for (size_t i = 0; i < ops->size(); ++i) {
      auto& up = ops->at(i);
      for (size_t j = i + 1; j < ops->size(); ++j) {
        auto& down = ops->at(j);
        for (int m = 0; m < up.output_size(); ++m) {
          auto output_event = up.output(m).event();
          for (int n = 0; n < down.trigger_size(); ++n) {
            if (down.trigger(n) != output_event) {
              continue;
            }
            down.add_upstream(i);
          }
        }
      }
    }
    for (size_t i = 0; i < ops->size(); ++i) {
      auto& up = ops->at(i);
      for (size_t j = i + 1; j < ops->size(); ++j) {
        auto& down = ops->at(j);
        for (int m = 0; m < up.output_size(); ++m) {
          auto up_output = up.mutable_output(m);
          auto output_event = up.output(m).event();
          for (int n = 0; n < down.trigger_size(); ++n) {
            if (down.trigger(n) != output_event) {
              continue;
            }
            auto ds = up_output->add_downstream();
            ds->set_op_id(j);
            ds->set_trigger_id(n);
            ds->set_event(down.trigger(n));
            ds->set_data(up_output->data());
            ds->set_type(up_output->type());
            if (n >= down.output_size()) {
              std::string data_name = up_output->data();
              if (up_output->downstream_size() > 1) {
                data_name += "_" + std::to_string(up_output->downstream_size() - 1)
                  + "_" + up.name() + "_END_COPY";
              }
              ds->set_data(data_name);
              down.set_trigger_data(n, data_name);
              continue;
            }
            std::string data_name;
            std::string type_name;
            auto& down_output = down.output(n);
            if (down_output.has_hz()) {
              ds->set_hz(down_output.hz());
            }
            if (!get_data_and_type_name(up.output(m), down_output, down.name(),
                                        down.trigger(n), data_name, type_name)) {
              LOG(ERROR) << "Failed to get data and type name." << down.name();
              return false;
            }

            CHECK(!data_name.empty()) << down.name() << ":" << up_output->DebugString();
            if (!down_output.has_data() && up_output->downstream_size() > 1) {
              data_name += "_" + std::to_string(up_output->downstream_size() - 1)
                + "_" + up.name() + "_COPY";
            }
            if (down_output.has_data()) {
              down.mutable_output(n)->set_data(down_output.data());
            } else {
              down.mutable_output(n)->set_data(data_name);
            }
            down.mutable_output(n)->set_type(type_name);
            down.set_trigger_data(n, data_name);
            ds->set_data(data_name);
            ds->set_type(type_name);
          }
        }
      }
    }

    return true;
  }

  static bool set_reference(std::vector<OperatorConfig>* ops) {
    for (size_t i = 0; i < ops->size(); ++i) {
      auto& a = ops->at(i);
      for (int j = 0; j < a.output_size(); ++j) {
        a.mutable_output(j)->set_has_reference(false);
      }
      for (size_t j = 0; j < ops->size(); ++j) {
        if (j == i) {
          continue;
        }
        auto& b = ops->at(j);
        for (int m = 0; m < a.output_size(); ++m) {
          auto output_event = a.output(m).event();
          for (int k = 0; k < b.input_size(); ++k) {
            auto input_event = b.input(k);
            if (output_event == input_event) {
              a.mutable_output(m)->set_has_reference(true);
            }
          }
          for (int k = 0; k < b.latest_size(); ++k) {
            auto latest_event = b.latest(k);
            if (output_event == latest_event) {
              a.mutable_output(m)->set_has_reference(true);
            }
          }
        }
      }
    }
    return true;
  }

  /**
   * @brief print the summary of the app dag
   * @param[in] the operator config list
   * @return the dag summary [std::string]
   */
  static std::string summary(const std::vector<OperatorConfig>& ops) {
    std::stringstream ss;
    for (size_t i = 0; i < ops.size(); ++i) {
      auto& op = ops[i];
      ss << std::right << std::setw(3) << i << ". " << std::left << std::setw(25) << op.name()
        << "   (ALG: " << op.algorithm() << ")" << std::endl;
      if (op.trigger_size() == op.trigger_data_size()) {
        ss << "  Trigger:" << std::endl;
        for (int j = 0; j < op.trigger_size(); ++j) {
          ss << "    * <" << op.trigger(j) << "> [" << op.trigger_data(j) << "]" << std::endl;
        }
      }
      ss << "  Output:" << std::endl;
      for (int j = 0; j < op.output_size(); ++j) {
        auto output_data = op.output(j).data();
        auto output_event = op.output(j).event();
        auto& op_output = op.output(j);

        ss << "    * <" << output_event << "> [" << output_data << "]" << std::endl;
        for (int k = 0; k < op_output.downstream_size(); ++k) {
          auto& downstream = op_output.downstream(k);
          auto& down_op = ops[downstream.op_id()];
          auto trigger_id = downstream.trigger_id();
          ss << "        -> " << std::left << std::setw(25) << down_op.name()
            << " [" << down_op.trigger_data(trigger_id) << "]" << std::endl;
        }
      }
    }

    return ss.str();
  }
};

}  // namespace airi
}  // namespace crdc
