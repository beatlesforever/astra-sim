/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/Ring.hh"

#include "astra-sim/system/PacketBundle.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"

using namespace AstraSim;

/**
 * @brief Ring 构造函数
 * @param type 通信类型（如 All-Reduce）
 * @param id 当前节点 ID
 * @param ring_topology 环形拓扑结构指针
 * @param data_size 传输的数据大小
 * @param direction 传输方向（顺时针或逆时针）
 * @param injection_policy 数据包注入策略
 */
Ring::Ring(ComType type,
           int id,
           RingTopology* ring_topology,
           uint64_t data_size,
           RingTopology::Direction direction,
           InjectionPolicy injection_policy)
    : Algorithm() {

    this->comType = type;  // 设置通信类型
    this->id = id;  // 当前节点 ID
    this->logical_topo = ring_topology;  // 赋值环形拓扑对象
    this->data_size = data_size;  // 传输的数据大小
    this->direction = direction;  // 传输方向
    this->nodes_in_ring = ring_topology->get_nodes_in_ring();  // 获取环形拓扑中的节点数
    this->curr_receiver = ring_topology->get_receiver(id, direction);  // 当前接收者
    this->curr_sender = ring_topology->get_sender(id, direction);  // 当前发送者
    this->parallel_reduce = 1;  // 默认设置并行 Reduce 操作数
    this->injection_policy = injection_policy;  // 数据包注入策略
    this->total_packets_sent = 0;  // 记录发送的数据包总数
    this->total_packets_received = 0;  // 记录接收的数据包总数
    this->free_packets = 0;  // 空闲数据包数
    this->zero_latency_packets = 0;  // 零延迟数据包数
    this->non_zero_latency_packets = 0;  // 非零延迟数据包数
    this->toggle = false;  // 切换状态
    this->name = Name::Ring;  // 设置算法名称为 "Ring"

    // 根据环形拓扑的维度设置数据传输方式
    if (ring_topology->get_dimension() == RingTopology::Dimension::Local) {
        transmition = MemBus::Transmition::Fast;  // 如果是本地维度，使用快速传输
    } else {
        transmition = MemBus::Transmition::Usual;  // 否则，使用常规传输
    }

    // 根据通信类型（All-Reduce、All-to-All等）确定数据流的数量
    switch (type) {
    case ComType::All_Reduce:
        stream_count = 2 * (nodes_in_ring - 1); 
        // All-Reduce 需要双向传输，每个节点都需要发送和接收数据
        break;
    case ComType::All_to_All:
        this->stream_count = ((nodes_in_ring - 1) * nodes_in_ring) / 2;
            // All-to-All 需要所有节点彼此通信，因此总流数量等于组合数 n(n-1)/2

        // 根据数据包的注入策略调整并行 Reduce 操作的规模
        switch (injection_policy) {
        case InjectionPolicy::Aggressive:
            this->parallel_reduce = nodes_in_ring - 1;
            // 激进模式下，所有可用节点并行执行 Reduce
            break;
        case InjectionPolicy::Normal:
            this->parallel_reduce = 1;
            // 普通模式下，每次只执行 1 个 Reduce 操作
            break;
        default:
            this->parallel_reduce = 1;  // 默认策略
            break;
        }
        break;
    default:
        stream_count = nodes_in_ring - 1;  // 其他通信类型默认使用 (n-1) 条流
    }
    // 确定最大流数，对于 All-to-All 和 All-Gather，最大流数设为 0（不限制）
    if (type == ComType::All_to_All || type == ComType::All_Gather) {
        max_count = 0;
    } else {
        max_count = nodes_in_ring - 1;
    }

    // 初始化剩余数据包计数
    remained_packets_per_message = 1;  // 每条消息的剩余数据包
    remained_packets_per_max_count = 1;  // 每个 max_count 的剩余数据包
    
    // 根据不同的通信类型计算最终数据大小和消息大小
    switch (type) {
    case ComType::All_Reduce:
        this->final_data_size = data_size; // 由于 All-Reduce 是一个归约操作，最终所有节点都会得到完整的 data_size 数据。
        this->msg_size = data_size / nodes_in_ring; //在 Reduce-Scatter 阶段，每个节点只需要处理 data_size/nodes_in_ring 的数据
        break;
    case ComType::All_Gather:
        this->final_data_size = data_size * nodes_in_ring; //最终每个节点都拥有 data_size * nodes_in_ring 的完整数据。
        this->msg_size = data_size; //每个节点最初只有 data_size，然后不断接收相同大小的数据，因此单次发送的数据量就是 data_size
        break;
    case ComType::Reduce_Scatter:
        this->final_data_size = data_size / nodes_in_ring; //由于数据是均分到所有 nodes_in_ring 个节点的，每个节点最终只保留 data_size / nodes_in_ring 大小的结果。
        this->msg_size = data_size / nodes_in_ring; //在 Reduce 过程中，每个节点都需要进行归约计算，每次传输的数据是 data_size / nodes_in_ring。
       // Reduce-Scatter 需要将数据分块并分配给不同的节点
        break;
    case ComType::All_to_All:
        this->final_data_size = data_size; //对于 All-to-All，最终数据量与初始数据量 data_size 相同，因为所有数据都分发出去了，但整体数据规模未变。
        this->msg_size = data_size / nodes_in_ring; //由于每个节点要给 nodes_in_ring - 1 个其他节点发送数据，因此每个单次发送的数据量为 data_size / nodes_in_ring。
        // All-to-All 每个节点发送给其他所有节点
        break;
    default:;
    }
}

/**
 * @brief 获取具有非零延迟的数据包数
 * 
 * @return 非零延迟数据包数，计算方式为 (nodes_in_ring - 1) * parallel_reduce
 */
int Ring::get_non_zero_latency_packets() {
    return (nodes_in_ring - 1) * parallel_reduce * 1;  // 计算非零延迟的数据包数量
}

/**
 * @brief 处理事件并执行相应操作
 * 
 * @param event 事件类型 (EventType)
 * @param data 事件数据指针 (CallData*)
 */
void Ring::run(EventType event, CallData* data) {
    if (event == EventType::General) {  
        free_packets += 1;  // 增加可用数据包数
        ready();  // 检查是否准备就绪
        iteratable();  // 检查是否可以继续迭代
    } else if (event == EventType::PacketReceived) {  
        total_packets_received++;  // 记录已接收数据包数量
        insert_packet(nullptr);  // 继续处理下一个数据包
    } else if (event == EventType::StreamInit) {  
        for (int i = 0; i < parallel_reduce; i++) {  
            insert_packet(nullptr);  // 初始化时插入多个数据包
        }
    }
}

/**
 * @brief 释放已锁定的数据包并发送它们
 */
void Ring::release_packets() {
    for (auto packet : locked_packets) {
        packet->set_notifier(this);  // 设置数据包的回调通知者
    }
    
    // 根据是否从 NPU 发送到 MA 进行相应的数据包发送
    if (NPU_to_MA == true) {
        (new PacketBundle(stream->owner, stream, locked_packets, processed,
                          send_back, msg_size, transmition))
            ->send_to_MA();  // 发送数据包到 MA（Memory Aggregator）
    } else {
        (new PacketBundle(stream->owner, stream, locked_packets, processed,
                          send_back, msg_size, transmition))
            ->send_to_NPU();  // 发送数据包到 NPU
    }
    
    locked_packets.clear();  // 清空已锁定数据包列表
}

/**
 * @brief 处理流计数，确保流能够正确进行
 */
void Ring::process_stream_count() {
    if (remained_packets_per_message > 0) {
        remained_packets_per_message--;  // 递减剩余数据包数
    }

    if (id == 0) {
        // 该部分代码为空，可能用于调试或后续扩展
    }

    if (remained_packets_per_message == 0 && stream_count > 0) {
        stream_count--;  // 递减流计数
        if (stream_count > 0) {
            remained_packets_per_message = 1;  // 继续处理下一消息
        }
    }

    // 如果数据包已完全发送完毕，并且流状态未死亡，则改变状态
    if (remained_packets_per_message == 0 && stream_count == 0 &&
        stream->state != StreamState::Dead) {
        stream->changeState(StreamState::Zombie);  // 标记流为 Zombie 状态
    }
}


/**
 * @brief 处理最大流计数，确保所有数据包能够被正确释放
 */
void Ring::process_max_count() {
    if (remained_packets_per_max_count > 0) {
        remained_packets_per_max_count--;  // 递减最大计数
    }

    if (remained_packets_per_max_count == 0) {
        max_count--;  // 递减最大流数
        release_packets();  // 释放数据包
        remained_packets_per_max_count = 1;  // 重置计数
    }
}

/**
 * @brief 处理 Reduce 操作，减少数据包并更新统计信息
 */
void Ring::reduce() {
    process_stream_count();  // 处理流计数
    packets.pop_front();  // 移除数据包
    free_packets--;  // 递减可用数据包数
    total_packets_sent++;  // 记录已发送数据包数
}

/**
 * @brief 判断是否可以进行下一次迭代
 * 
 * @return 如果迭代结束返回 false，否则返回 true
 */
bool Ring::iteratable() {
    if (stream_count == 0 && free_packets == (parallel_reduce * 1)) {  
        exit();  // 退出当前流程
        return false;
    }
    return true;
}

/**
 * @brief 向数据包队列中插入一个新的数据包
 * @param sender 数据包的发送者（可以为空）
 */
void Ring::insert_packet(Callable* sender) {
    // 如果零延迟和非零延迟数据包都已用完，则重新计算
    if (zero_latency_packets == 0 && non_zero_latency_packets == 0) {
        zero_latency_packets = parallel_reduce * 1;  // 计算零延迟数据包数量
        non_zero_latency_packets = get_non_zero_latency_packets();  // 计算非零延迟数据包数量
        toggle = !toggle;  // 切换传输模式
    }

    // 处理零延迟数据包
    if (zero_latency_packets > 0) {
        packets.push_back(MyPacket(
            stream->current_queue_id, curr_sender, curr_receiver));  // 创建新数据包

        packets.back().sender = sender;  // 记录数据包的发送者
        locked_packets.push_back(&packets.back());  // 将数据包加入锁定队列
        processed = false;  // 标记该数据包未处理
        send_back = false;  // 不需要回传数据
        NPU_to_MA = true;  // 传输方向设为 NPU -> Memory Aggregator (MA)
        process_max_count();  // 处理最大流计数
        zero_latency_packets--;  // 递减零延迟数据包计数
        return;
    } // 处理非零延迟数据包
    else if (non_zero_latency_packets > 0) {
        packets.push_back(MyPacket(
            stream->current_queue_id, curr_sender,
            curr_receiver));  // vnet Must be changed for alltoall topology

        packets.back().sender = sender;  // 记录数据包的发送者
        locked_packets.push_back(&packets.back());  // 将数据包加入锁定队列

        // 如果是 Reduce-Scatter 或 All-Reduce（在 `toggle` 开启的情况下），则需要处理
        if (comType == ComType::Reduce_Scatter ||
            (comType == ComType::All_Reduce && toggle)) {
            processed = true;  // 数据需要处理
        } else {
            processed = false;  // 直接传输数据
        }
    
        // 判断是否需要回传数据
        if (non_zero_latency_packets <= parallel_reduce * 1) {
            send_back = false;  // 不回传数据
        } else {
            send_back = true;  // 需要回传数据
        }

        NPU_to_MA = false;  // 传输方向设为 NPU -> NPU
        process_max_count();  // 处理最大流计数
        non_zero_latency_packets--;  // 递减非零延迟数据包计数
        return;
    }
    // 如果程序执行到这里，说明未能成功插入数据包，触发系统错误
    Sys::sys_panic("should not inject nothing!");
}

/**
 * @brief 检查流是否准备就绪，并发送数据包
 * @return 如果流可执行返回 true，否则返回 false
 */
bool Ring::ready() {
    // 如果流处于 `Created` 或 `Ready` 状态，则将其状态更改为 `Executing`
    if (stream->state == StreamState::Created ||
        stream->state == StreamState::Ready) {
        stream->changeState(StreamState::Executing);
    }

    // 如果数据包队列为空，或者没有剩余流，或者没有空闲数据包，则返回 false
    if (packets.size() == 0 || stream_count == 0 || free_packets == 0) {
        return false;
    }

    // 获取队列中的第一个数据包
    MyPacket packet = packets.front();

    // 发送数据包请求
    sim_request snd_req;
    snd_req.srcRank = id;  // 发送方 ID
    snd_req.dstRank = packet.preferred_dest;  // 目标 ID
    snd_req.tag = stream->stream_id;  // 关联流 ID
    snd_req.reqType = UINT8;  // 数据类型
    snd_req.vnet = this->stream->current_queue_id;  // 虚拟网络 ID

    // 通过仿真前端 API 发送数据包
    stream->owner->front_end_sim_send(
        0, Sys::dummy_data, msg_size, UINT8, packet.preferred_dest,
        stream->stream_id, &snd_req, Sys::FrontEndSendRecvType::COLLECTIVE,
        &Sys::handleEvent, nullptr); // stream_id+(packet.preferred_dest*50)

    // 处理接收数据包请求
    sim_request rcv_req;
    rcv_req.vnet = this->stream->current_queue_id;

    // 创建事件处理数据结构
    RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
        stream, stream->owner->id, EventType::PacketReceived,
        packet.preferred_vnet, packet.stream_id);

    // 通过仿真前端 API 接收数据包
    stream->owner->front_end_sim_recv(
        0, Sys::dummy_data, msg_size, UINT8, packet.preferred_src,
        stream->stream_id, &rcv_req, Sys::FrontEndSendRecvType::COLLECTIVE,
        &Sys::handleEvent, ehd); // stream_id+(owner->id*50)

    // 执行 Reduce 操作
    reduce();

    return true;
}

/**
 * @brief 退出当前流，清理数据并准备进入下一个流
 */
void Ring::exit() {
    // 如果数据包队列不为空，则清空
    if (packets.size() != 0) {
        packets.clear();
    }

    // 如果锁定的数据包队列不为空，则清空
    if (locked_packets.size() != 0) {
        locked_packets.clear();
    }

    // 继续执行下一个虚拟网络流
    stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
    return;
}
