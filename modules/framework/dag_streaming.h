// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: anlysis the dag file stream and create the app start thread

#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "framework/framework.h"

namespace crdc {
namespace airi {

DECLARE_int32(max_allowed_congestion_value);
DECLARE_bool(enable_timing_remove_stale_data);

/**
 * @brief This Class is used to create the app by dag file.
 *        So it is used to read the day config file, analysis it
 *        and create the data pipline.
 */
class DAGStreaming : public crdc::airi::common::Thread {
 public:
  DAGStreaming();
  virtual ~DAGStreaming();
  /**
   * @brief init the dag thread
   * @param the path of the dag config file [std::string]
   */
  bool init(const std::string& dag_config_path);

  /**
   * @brief the stop function
   */
  void stop();

  /**
   * @brief to reset the dag streaming
   */
  void reset();

 protected:
  /**
   * @brief the run function of the dag streaming thread
   */
  void run() override;

  /**
   * @brief Analysis the content of the dag config file.
   * @param the element of the dag file in proto style [proto class]
   */
  bool parse_config(const DAGConfig& dag_config);

  /**
   * @brief Registe all the operator in the DAG Streaming
   * @param the list of the operator configuration [std::vector]
   */
  bool registe_data(const std::vector<OperatorConfig>& ops);

  /**
   * @brief Registe all the data
   * @param the name of the data [std::string]
   * @param the input frequence [int]
   * @param the type of the data [std::string]
   */
  bool registe_data(std::string data_name, int hz, std::string data_type);

 private:
  /**
   * @brief schedule all the operator and data operation.
   *        It is used directly in the run method.
   */
  void schedule();

  /**
   * @brief init the dag
   * @return if the init successed[bool]
   */
  bool init_dag();

  /**
   * @brief Some cache data is stored. If the remove staled data flag is opened.
   * The staled data could be removed with this method.
   */
  void remove_stale_data();

  EventManager* event_manager_ = nullptr;
  SharedDataManager* shared_data_manager_ = nullptr;
  bool inited_;
  volatile bool stop_;
  DAGConfig config_;
  std::vector<std::shared_ptr<Operator>> ops_;
  std::vector<EventMeta> events_;
  std::vector<std::vector<EventMeta>> operator_sub_events_;
  std::vector<std::vector<std::vector<EventMeta>>> operator_pub_events_;
  DISALLOW_COPY_AND_ASSIGN(DAGStreaming);
};

}  // namespace airi
}  // namespace crdc
