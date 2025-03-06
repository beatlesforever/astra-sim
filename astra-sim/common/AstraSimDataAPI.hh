/******************************************************************************
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *******************************************************************************/
// 该代码基于 MIT 许可证进行开源，具体的许可证信息可在项目根目录的 LICENSE 文件中查看。

#ifndef __ASTRA_SIM_DATA_API_HH__
#define __ASTRA_SIM_DATA_API_HH__

#include <list>    // 用于存储有序的层统计数据
#include <string>  // 用于存储层的名称
#include <vector>  // 用于存储逻辑维度的平均延迟数据

namespace AstraSim {

/**
 * @class LayerData
 * @brief 存储单个层的计算和通信统计信息
 * 
 * 该类用于记录模拟过程中，每一层（Layer）的计算时间、通信时间以及排队延迟等信息。
 */
class LayerData {
  public:
    std::string layer_name; ///< 层名称，用于标识该层

    // 计算时间（单位：秒）
    double total_forward_pass_compute; ///< 前向传播的计算时间
    double total_weight_grad_compute;  ///< 权重梯度计算时间
    double total_input_grad_compute;   ///< 输入梯度计算时间

    // 等待通信的时间（单位：秒）
    double total_waiting_for_fwd_comm; ///< 等待前向传播通信完成的时间
    double total_waiting_for_wg_comm;  ///< 等待权重梯度通信完成的时间
    double total_waiting_for_ig_comm;  ///< 等待输入梯度通信完成的时间

    // 通信时间（单位：秒）
    double total_fwd_comm; ///< 前向传播的总通信时间
    double total_weight_grad_comm; ///< 权重梯度的总通信时间
    double total_input_grad_comm; ///< 输入梯度的总通信时间

    // 网络排队和消息传输延迟（单位：秒）
    std::list<std::pair<int, double>>
        avg_queuing_delay;  ///< 平均排队延迟（phase_id, latency）
    std::list<std::pair<int, double>>
        avg_network_message_dealy;  ///< 平均网络消息传输延迟（phase_id, latency）
};

/**
 * @class AstraSimDataAPI
 * @brief 存储 AstraSim 模拟过程中收集的统计数据
 * 
 * 该类用于存储整个模拟过程的统计信息，包括各层的计算和通信数据，以及整体的计算和通信时间。
 */
class AstraSimDataAPI {
  public:
    std::string run_name; ///< 运行名称，用于标识该模拟实验

    std::list<LayerData> layers_stats; ///< 记录所有层的统计数据

    std::vector<double> avg_chunk_latency_per_logical_dimension;
    ///< 逻辑维度上的平均数据块延迟，用于评估不同通信维度的网络性能。

    double workload_finished_time; ///< 记录整个模拟工作负载完成的时间（单位：秒）

    double total_compute; ///< 记录整个模拟过程中的计算时间（单位：秒）

    double total_exposed_comm; ///< 记录整个模拟过程中的通信时间（单位：秒）
};

}  // namespace AstraSim

#endif /* __ASTRA_SIM_DATA_API_HH__ */
