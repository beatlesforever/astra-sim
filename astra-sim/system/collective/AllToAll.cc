/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/AllToAll.hh"

using namespace AstraSim; // 使用 AstraSim 命名空间

/**
 * @brief AllToAll 构造函数，初始化 All-To-All 集合通信算法
 * @param type 通信类型，例如 All_to_All, All_Reduce 等
 * @param window 窗口大小 (window) 主要影响 一次可以并行进行多少次数据传输。
 * @param id 进程 ID
 * @param allToAllTopology 绑定的 RingTopology（环形拓扑）
 * @param data_size 处理的数据大小
 * @param direction 数据流的方向（顺时针或逆时针）
 * @param injection_policy 控制数据注入策略（如 Aggressive 或 Normal）
 */
AllToAll::AllToAll(ComType type,
                   int window,
                   int id,
                   RingTopology* allToAllTopology,
                   uint64_t data_size,
                   RingTopology::Direction direction,
                   InjectionPolicy injection_policy)
    : Ring(type, id, allToAllTopology, data_size, direction, injection_policy) {
    this->name = Name::AllToAll; // 设置算法名称
    this->middle_point = nodes_in_ring - 1; // 计算环形拓扑的中间节点索引
    if (window == -1) { 
        // 如果 window 参数为 -1，则默认设置为环内节点数量 - 1
        parallel_reduce = nodes_in_ring - 1;
    } else {
        // 否则，取 window 和 (nodes_in_ring - 1) 的最小值
        parallel_reduce = (int)std::min(window, nodes_in_ring - 1);
    }
    if (type == ComType::All_to_All) {
        this->stream_count = nodes_in_ring - 1; // All-to-All 操作需要 (N-1) 轮传输
    }
}

/**
 * @brief 运行 All-To-All 通信算法
 * @param event 事件类型（数据包接收、流初始化等）
 * @param data 事件相关数据
 */
void AllToAll::run(EventType event, CallData* data) {
    if (event == EventType::General) { // 普通事件处理
        free_packets += 1; // 增加可用的数据包数量

        // 处理 All-Reduce 逻辑
        if (comType == ComType::All_Reduce && stream_count <= middle_point) {
            if (total_packets_received < middle_point) {
                return;  // 若未接收到足够的数据包，则不进行后续处理
            }
            for (int i = 0; i < parallel_reduce; i++) {
                ready(); // 检查是否可以继续执行
            }
            iteratable(); // 确保算法可以继续迭代
        } else {
            ready();
            iteratable();
        }

    } else if (event == EventType::PacketReceived) { // 数据包接收事件
        total_packets_received++; // 记录接收的数据包数
        insert_packet(nullptr); // 插入一个新的数据包

    } else if (event == EventType::StreamInit) {  // 初始化数据流事件
        for (int i = 0; i < parallel_reduce; i++) {
            insert_packet(nullptr); // 按照并行度插入多个数据包
        }
    }
}

/**
 * @brief 处理最大数据包计数逻辑
 */
void AllToAll::process_max_count() {
    if (remained_packets_per_max_count > 0) {
        remained_packets_per_max_count--; // 递减剩余数据包计数
    }

    if (remained_packets_per_max_count == 0) {
        max_count--; // 递减最大计数
        release_packets(); // 释放已处理的数据包
        remained_packets_per_max_count = 1; // 重置计数

        // 更新当前接收者
        curr_receiver = ((RingTopology*)logical_topo)
                            ->get_receiver(curr_receiver, direction);
        if (curr_receiver == id) {
            // 如果当前接收者等于自身，则再往前找下一个接收者
            curr_receiver = ((RingTopology*)logical_topo)
                                ->get_receiver(curr_receiver, direction);
        }

        // 更新当前发送者
        curr_sender =
            ((RingTopology*)logical_topo)->get_sender(curr_sender, direction);
        if (curr_sender == id) {
            // 如果当前发送者等于自身，则再往前找下一个发送者
            curr_sender = ((RingTopology*)logical_topo)
                              ->get_sender(curr_sender, direction);
        }
    }
}

/**
 * @brief 计算非零延迟的数据包数量
 * @return 非零延迟数据包的数量
 */
int AllToAll::get_non_zero_latency_packets() {
    if (((RingTopology*)logical_topo)->get_dimension() !=
        RingTopology::Dimension::Local) {
        return parallel_reduce * 1; // 若非本地环，则直接返回并行减少值
    } else {
        return (nodes_in_ring - 1) * parallel_reduce * 1; // 本地环中需要更多的数据包
    }
}
