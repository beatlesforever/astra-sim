/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __WORKLOAD_HH__
#define __WORKLOAD_HH__

#include <memory>  // 引入智能指针 std::shared_ptr
#include <string>  // 引入字符串处理 std::string
#include <unordered_map>  // 引入哈希映射 std::unordered_map

// 包含 Astra-Sim 相关头文件
#include "astra-sim/system/Callable.hh"  // 继承自 Callable 类（事件回调机制）
#include "astra-sim/system/CommunicatorGroup.hh"  // 处理通信组
#include "astra-sim/workload/HardwareResource.hh"  // 处理硬件资源管理
#include "extern/graph_frontend/chakra/src/feeder/et_feeder.h"  // 任务调度器 ETFeeder

namespace AstraSim {

// 前向声明类，避免不必要的包含
class Sys;  // 分布式系统管理类
class DataSet;  // 任务数据集类

/**
 * @brief Workload 类，负责管理计算任务调度，包括计算、内存访问和通信操作。
 *        继承自 Callable 类，支持事件回调机制。
 */
class Workload : public Callable {
  public:
   /**
     * @brief Workload 构造函数
     * 
     * @param sys 指向系统对象的指针
     * @param et_filename 计算任务的输入文件名（execution trace 文件）
     * @param comm_group_filename 通信组配置文件
     */
    Workload(Sys* sys,
             std::string et_filename,
             std::string comm_group_filename);

    /**
     * @brief Workload 析构函数
     *        释放资源（ETFeeder、CommunicatorGroup、HardwareResource 等）。
     */
    ~Workload();

    // communicator groups
    // ** 通信组初始化 **
    /**
     * @brief 初始化通信组
     * 
     * @param comm_group_filename 通信组配置文件路径
     */
    void initialize_comm_group(std::string comm_group_filename);

    // ** 事件驱动模拟 **
    /**
     * @brief 处理无依赖的任务节点，并将可执行的任务调度出去
     */
    void issue_dep_free_nodes();

    /**
     * @brief 处理任务的执行
     * 
     * @param node 需要执行的任务节点
     */
    void issue(std::shared_ptr<Chakra::ETFeederNode> node);

     /**
     * @brief 回放任务，直接按照记录的执行时间模拟任务执行
     * 
     * @param node 需要回放执行的任务节点
     */
    void issue_replay(std::shared_ptr<Chakra::ETFeederNode> node);

/**
     * @brief 处理远程内存访问任务
     * 
     * @param node 需要执行的任务节点
     */
    void issue_remote_mem(std::shared_ptr<Chakra::ETFeederNode> node);

    /**
     * @brief 处理计算任务 (CPU 或 GPU)
     * 
     * @param node 需要执行的计算任务节点
     */
    void issue_comp(std::shared_ptr<Chakra::ETFeederNode> node);

    /**
     * @brief 处理通信任务，包括集合通信 (Collective Communication) 和点对点通信
     * 
     * @param node 需要执行通信的任务节点
     */
    void issue_comm(std::shared_ptr<Chakra::ETFeederNode> node);

    /**
     * @brief 处理无效任务，释放资源
     * 
     * @param node 需要跳过的任务节点
     */
    void skip_invalid(std::shared_ptr<Chakra::ETFeederNode> node);

    /**
     * @brief 事件回调函数，在任务完成后被触发
     * 
     * @param event 事件类型
     * @param data 事件回调数据
     */
    void call(EventType event, CallData* data);

    /**
     * @brief 触发任务调度
     */
    void fire();

    // ** 统计与报告 **
    /**
     * @brief 记录 Workload 执行的统计信息，包括执行时间和通信时间
     */
    void report();

    // ** 成员变量 **

    Chakra::ETFeeder* et_feeder;  // ETFeeder 任务调度器，用于管理任务队列
    CommunicatorGroup* comm_group;  // 负责管理该 Workload 所属的通信组
    HardwareResource* hw_resource;  // 该 Workload 运行时使用的硬件资源
    Sys* sys;  // 指向系统管理对象的指针

    // ** 记录集合通信任务 **
    std::unordered_map<int, uint64_t> collective_comm_node_id_map; 
    // 存储集合通信任务的映射关系 (DataSet ID -> ETFeederNode ID)

    std::unordered_map<int, DataSet*> collective_comm_wrapper_map;
    // 存储 DataSet 对象的指针，确保在任务完成后正确释放资源

    bool is_finished;  // 标志 Workload 是否完成
};

}  // namespace AstraSim

#endif /* __WORKLOAD_HH__ */
