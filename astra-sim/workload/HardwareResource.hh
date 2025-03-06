/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

// TODO: HardwareResource.hh should be moved to the system layer.

#ifndef __HARDWARE_RESOURCE_HH__
#define __HARDWARE_RESOURCE_HH__

#include <cstdint>

#include "extern/graph_frontend/chakra/src/feeder/et_feeder.h" // 引入 Chakra 框架的任务调度器

namespace AstraSim { // 定义 AstraSim 命名空间

/**
 * @brief HardwareResource 类用于管理计算资源的使用情况，包括 CPU 和 GPU 任务。
 * 
 * 该类用于跟踪 CPU 和 GPU 计算任务的执行情况，确保任务调度不会发生冲突，
 * 并提供方法来查询资源的可用性和输出当前的计算资源统计信息。
 */
class HardwareResource {
  public:

   /**
     * @brief 构造函数，初始化硬件资源管理器
     * 
     * @param num_npus 指定系统中可用的 NPU（神经处理单元）数量
     */
    HardwareResource(uint32_t num_npus);

    /**
     * @brief 占用计算资源
     * 
     * @param node 需要占用的计算任务节点
     */
    void occupy(const std::shared_ptr<Chakra::ETFeederNode> node);

/**
     * @brief 释放计算资源
     * 
     * @param node 需要释放的计算任务节点
     */
    void release(const std::shared_ptr<Chakra::ETFeederNode> node);

    /**
     * @brief 检查计算资源是否可用
     * 
     * @param node 需要检查的计算任务节点
     * @return true 资源可用
     * @return false 资源不可用
     */
    bool is_available(const std::shared_ptr<Chakra::ETFeederNode> node) const;

    /**
     * @brief 输出当前计算资源的使用情况
     */
    void report();

    // 指向当前正在执行的计算任务节点
    std::shared_ptr<Chakra::ETFeederNode> cpu_ops_node; // 当前正在执行的 CPU 计算任务
    std::shared_ptr<Chakra::ETFeederNode> gpu_ops_node; // 当前正在执行的 GPU 计算任务
    std::shared_ptr<Chakra::ETFeederNode> gpu_comms_node; // 当前正在执行的 GPU 通信任务

    const uint32_t num_npus; // 系统中可用的 NPU（神经处理单元）数量

    // 当前正在执行的任务数量
    uint32_t num_in_flight_cpu_ops; // 当前正在执行的 CPU 计算任务数量
    uint32_t num_in_flight_gpu_comp_ops; // 当前正在执行的 GPU 计算任务数量
    uint32_t num_in_flight_gpu_comm_ops; // 当前正在执行的 GPU 通信任务数量

    // 统计累计执行的任务总数
    uint64_t num_cpu_ops; // CPU 计算任务的总执行次数
    uint64_t num_gpu_ops; // GPU 计算任务的总执行次数
    uint64_t num_gpu_comms; // GPU 通信任务的总执行次数

    // 计算任务的执行时间（时钟周期）
    uint64_t tics_cpu_ops; // CPU 计算任务的总执行时钟周期
    uint64_t tics_gpu_ops; // GPU 计算任务的总执行时钟周期
    uint64_t tics_gpu_comms; // GPU 通信任务的总执行时钟周期
};

}  // namespace AstraSim

#endif /* __HARDWARE_RESOURCE_HH__ */
