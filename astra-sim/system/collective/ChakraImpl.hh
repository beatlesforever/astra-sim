/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __CHAKRAIMPL__HH
#define __CHAKRAIMPL__HH

#include <stdlib.h>
#include <unistd.h>

// AstraSim 相关头文件
#include "astra-sim/system/MemBus.hh"   // 内存总线模拟
#include "astra-sim/system/MyPacket.hh" // 数据包结构定义
#include "astra-sim/system/collective/Algorithm.hh" // 集合通信算法的基类
#include "extern/graph_frontend/chakra/src/feeder/et_feeder.h" // Chakra ET 执行轨迹加载器

namespace AstraSim {

/**
 * @brief 硬件资源管理类
 * 
 * 该类用于管理 Chakra 执行轨迹（ET）中的计算和通信资源，防止资源冲突。
 */
class HardwareResourceChakra {
  public:
    HardwareResourceChakra();  // 构造函数，初始化硬件资源状态

    /**
     * @brief 占用某个 ETFeederNode 代表的计算或通信资源
     * @param node 指向 ETFeederNode 的共享指针
     */
    void occupy(const std::shared_ptr<Chakra::ETFeederNode> node);

    /**
     * @brief 释放某个 ETFeederNode 代表的计算或通信资源
     * @param node 指向 ETFeederNode 的共享指针
     */
    void release(const std::shared_ptr<Chakra::ETFeederNode> node);

    /**
     * @brief 检查某个 ETFeederNode 代表的计算或通信资源是否可用
     * @param node 指向 ETFeederNode 的共享指针
     * @return 若资源可用则返回 true，否则返回 false
     */
    bool is_available(const std::shared_ptr<Chakra::ETFeederNode> node) const;

    // 当前正在执行的 CPU 计算操作数
    uint32_t num_in_flight_cpu_ops;
    // 当前正在执行的 GPU 计算操作数
    uint32_t num_in_flight_gpu_comp_ops;
    // 当前正在执行的 GPU 通信操作数
    uint32_t num_in_flight_gpu_comm_ops;
};

/*
 * ChakraImpl contains the logic to parse and execute a Chakra ET representation
 * of a collective implementation. (instead of the typical way of implementing
 * collectives within the system layer and executing the code.)
 *
 * This Chakra trace consists of only COMM_SEND, COMM_RECV, COMP nodes to
 * describe the messages that make up a Collective. (note: no verifier!) This
 * allows us to easily define different implementations of the same algorithm,
 * or, even define our own custom algorithms using tools such as MSCCLang. To
 * use this implementation, write the "ABSOLUTE" path to the Chakra trace file
 * in the system layer input (instead of traditional `Ring`, etc.), under
 * `{all-reduce|all-to-all|all-gather}-implementation-chakra`.
 *
 * For a detailed description on using a Chakra ET based representation, refer
 * to the documentation.
 * TODO: Add a verifier to verify correct communication behavior.
 */

/**
 * @brief ChakraImpl 类：解析并执行 Chakra ET（执行轨迹）集合通信算法
 *
 * 该类实现了一种基于 Chakra 执行轨迹（ET）的集合通信方式，
 * 而不是在系统层直接实现集合通信逻辑。它解析 ET 并执行 COMM_SEND, COMM_RECV, COMP 三类节点，
 * 从而定义了不同的集合通信算法的实现方式（如 All-Reduce、All-Gather 等）。
 *
 * - 该 ET 仅包含通信（COMM_SEND、COMM_RECV）和计算（COMP）节点。
 * - 不包含验证器（verifier），意味着不会检测通信正确性。
 * - 允许使用工具（如 MSCCLang）自定义集合通信算法。
 * - 需要在系统层输入文件中指定绝对路径 `{all-reduce|all-to-all|all-gather}-implementation-chakra` 来使用此实现。
 *
 * TODO: 添加一个验证器来检查通信行为是否正确。
 */
class ChakraImpl : public Algorithm {
  public:

    /**
     * @brief ChakraImpl 构造函数
     * @param et_filename 执行轨迹文件的路径
     * @param id 该节点的唯一 ID
     */
    ChakraImpl(std::string et_filename, int id);

    // Runs the collective algorithm. This function is only called once to start
    // the algorithm.
    /**
     * @brief 运行集合通信算法
     *
     * 该函数仅调用一次，用于启动整个集合通信算法的执行。
     *
     * @param event 事件类型
     * @param data 事件数据
     */
    virtual void run(EventType event, CallData* data);

    /**
     * @brief 事件回调函数
     *
     * 当某个计算或通信操作完成后，该函数会被回调，并继续调度后续的执行轨迹节点。
     *
     * @param event 事件类型
     * @param data 事件数据
     */
    void call(EventType event, CallData* data);

  private:
    /*
     * The following functions move through the Chakra ET and issues nodes whose
     * dependencies are resolved, Similar to how the Workload layer moves
     * through the workload Chakra ET.
     * TODO: merge with impl in Workload layer.
     */
    // 下面的函数会遍历 Chakra 执行轨迹（ET），并调度那些 依赖已解决 的节点执行，
    // 这个过程类似于 Workload 层 如何调度它的 ET（执行轨迹）。
    // TODO: 需要将此实现与 Workload 层的实现合并，避免重复代码。

    /**
     * @brief 解析并执行一个 ETFeederNode
     *
     * 该函数用于遍历执行轨迹中的各个节点，并根据节点类型（COMM_SEND, COMM_RECV, COMP）分别执行。
     *
     * @param node 指向 ETFeederNode 的共享指针
     */
    void issue(std::shared_ptr<Chakra::ETFeederNode> node);

     /**
     * @brief 执行所有无依赖的节点
     *
     * 在执行轨迹中查找所有已经满足依赖关系的节点，并执行它们。
     */
    void issue_dep_free_nodes();

    // Rank Id
    int id;
    // ET Feeder for the Chakra ET for this specific rank.
    // Chakra ET 执行轨迹加载器（用于解析并管理 ET）
    Chakra::ETFeeder* et_feeder;
    // Tracks availability of hardware resources (e.g. prevent two send ET nodes
    // at same time).
    // TODO: merge with impl in Workload layer.

    /**
     * @brief 硬件资源管理
     *
     * 用于跟踪计算和通信资源的可用性，防止同一时间执行多个冲突的操作
     * 例如，避免同一时间调度两个发送（COMM_SEND）操作。
     *
     * TODO: 该功能可能与 Workload 层的实现存在重复，后续可合并。
     */
    HardwareResourceChakra* hw_resource;
};

}  // namespace AstraSim

#endif /* __CHAKRAIMPL__HH */
