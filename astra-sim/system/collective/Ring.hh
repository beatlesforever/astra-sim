/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __RING_HH__
#define __RING_HH__

#include "astra-sim/system/MemBus.hh"
#include "astra-sim/system/MyPacket.hh"
#include "astra-sim/system/collective/Algorithm.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim {

/**
 * @class Ring
 * @brief 实现基于环形拓扑的集合通信算法。
 */
class Ring : public Algorithm {
  public:
    /**
     * @brief 构造函数，初始化 Ring 算法。
     * @param type 通信类型（如 All-Reduce）。
     * @param id 当前节点 ID。
     * @param ring_topology 指向环形拓扑对象的指针。
     * @param data_size 需要传输的数据大小（字节）。
     * @param direction 通信方向（顺时针或逆时针）。
     * @param injection_policy 数据包注入策略。
     */
    Ring(ComType type,
         int id,
         RingTopology* ring_topology,
         uint64_t data_size,
         RingTopology::Direction direction,
         InjectionPolicy injection_policy);

    /**
     * @brief 运行 Ring 算法。
     * @param event 事件类型。
     * @param data 事件数据指针。
     */
    virtual void run(EventType event, CallData* data);

    /**
     * @brief 处理数据流计数。
     */
    void process_stream_count();

    /**
     * @brief 释放已完成的数据包。
     */
    void release_packets();

    /**
     * @brief 处理最大可并行数据包数。
     */
    virtual void process_max_count();

    /**
     * @brief 执行 Reduce 操作。
     */
    void reduce();

    /**
     * @brief 判断是否可以进行下一轮迭代。
     * @return 如果可以迭代返回 true，否则返回 false。
     */
    bool iteratable();

    /**
     * @brief 获取具有非零延迟的数据包数量。
     * @return 非零延迟数据包数量。
     */
    virtual int get_non_zero_latency_packets();

    /**
     * @brief 将数据包插入发送队列。
     * @param sender 发送方指针。
     */
    void insert_packet(Callable* sender);

    /**
     * @brief 判断是否所有节点都准备就绪。
     * @return 如果准备就绪返回 true，否则返回 false。
     */
    bool ready();

    /**
     * @brief 退出 Ring 算法。
     */
    void exit();

    // 通信维度方向（冗余变量，与 direction 作用相同）
    RingTopology::Direction dimension;

    // 当前通信方向
    RingTopology::Direction direction;

    // 传输方式
    MemBus::Transmition transmition;

    // 零延迟数据包数
    int zero_latency_packets;

    // 非零延迟数据包数
    int non_zero_latency_packets;

    // 当前节点 ID
    int id;

    // 当前接收方节点 ID
    int curr_receiver;

    // 当前发送方节点 ID
    int curr_sender;

    // 环形拓扑中的总节点数
    int nodes_in_ring;

    // 数据流计数
    int stream_count;

    // 允许的最大数据流计数
    int max_count;

    // 每个 max_count 剩余的数据包数
    int remained_packets_per_max_count;

    // 每条消息剩余的数据包数
    int remained_packets_per_message;

    // 是否支持并行 Reduce 操作
    int parallel_reduce;

    // 数据包注入策略
    InjectionPolicy injection_policy;

    // 发送/接收的数据包列表
    std::list<MyPacket> packets;

    // 数据包发送/接收状态切换
    bool toggle;

    // 可用的数据包数
    long free_packets;

    // 发送的数据包总数
    long total_packets_sent;

    // 接收的数据包总数
    long total_packets_received;

    // 消息大小（字节）
    uint64_t msg_size;

    // 已锁定的数据包列表
    std::list<MyPacket*> locked_packets;

    // 是否已处理完成
    bool processed;

    // 是否需要返回数据包
    bool send_back;

    // 是否进行 NPU 到 MA 的传输
    bool NPU_to_MA;
};

}  // namespace AstraSim

#endif /* __RING_HH__ */
