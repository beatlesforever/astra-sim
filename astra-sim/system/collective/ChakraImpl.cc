/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>

// 包含Astra-Sim相关头文件
#include "astra-sim/system/RecvPacketEventHandlerData.hh"  // 处理接收数据包事件
#include "astra-sim/system/SendPacketEventHandlerData.hh"  // 处理发送数据包事件
#include "astra-sim/system/collective/ChakraImpl.hh"  // Chakra算法实现
#include "extern/graph_frontend/chakra/src/feeder/et_feeder.h"  // Chakra ET Feeder，解析Chakra ET数据

using namespace std;
using namespace AstraSim;
using namespace Chakra;

// 定义Chakra的节点类型枚举
typedef ChakraProtoMsg::NodeType ChakraNodeType;

// ChakraImpl 构造函数，接收ET（Execution Trace）文件名和ID作为参数
ChakraImpl::ChakraImpl(std::string et_filename, int id) : Algorithm() {
    this->et_feeder = new Chakra::ETFeeder(et_filename);  // 初始化 ET Feeder，解析ET文件
    this->id = id;  // 记录当前节点的ID
}

/**
 * @brief 处理执行轨迹节点的调度
 * @param node 指向 ETFeederNode 的共享指针
 */
void ChakraImpl::issue(shared_ptr<Chakra::ETFeederNode> node) {

    ChakraNodeType type = node->type(); // 获取当前节点的类型

    if (type == ChakraNodeType::COMM_SEND_NODE) { // 如果是发送数据包节点
        sim_request snd_req; // 定义一个模拟请求结构体
        snd_req.srcRank = node->comm_src(); // 发送方的 ID
        snd_req.dstRank = node->comm_dst(); // 接收方的 ID
        snd_req.reqType = UINT8; // 设置请求的数据类型为 8 位无符号整数

        // 创建并初始化发送数据包事件处理数据
        SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
        sehd->callable = this; // 绑定回调处理对象
        sehd->wlhd = new WorkloadLayerHandlerData;
        sehd->wlhd->node_id = node->id(); // 记录当前节点 ID
        sehd->event = EventType::PacketSent; // 设置事件类型为数据包已发送
        
        // 触发前端模拟发送数据包
        stream->owner->front_end_sim_send(
            0, Sys::dummy_data, // 发送的数据
            // Note that we're using the comm size as hardcoded in the Impl
            // Chakra et, ed through the comm. api, and ignore the comm.size fed
            // in the workload chakra et. TODO: fix.
            
            //请注意，我们在实现（Impl）中硬编码了 comm_size，并通过 Chakra API 传递它，而忽略了 工作负载（workload）中的 comm_size。
            //TODO: 需要修复这个问题。
            node->comm_size(), UINT8, node->comm_dst(), node->comm_tag(), // 发送大小、类型、目标、标签
            &snd_req, Sys::FrontEndSendRecvType::NATIVE, &Sys::handleEvent, // 请求信息、类型和事件处理函数
            sehd); // 事件数据

    } else if (type == ChakraNodeType::COMM_RECV_NODE) {// 如果是接收数据包节点
        sim_request rcv_req; // 定义接收请求结构体

        // 创建并初始化接收数据包事件处理数据
        RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;

        rcehd->wlhd = new WorkloadLayerHandlerData;
        rcehd->wlhd->node_id = node->id(); // 记录当前节点 ID
        rcehd->chakra = this; // 绑定回调处理对象
        rcehd->event = EventType::PacketReceived; // 设定事件类型为数据包已接收
        
        // 触发前端模拟接收数据包
        stream->owner->front_end_sim_recv(
            0, Sys::dummy_data, node->comm_size(), UINT8, node->comm_src(), // 接收数据、大小、类型、来源
            node->comm_tag(), &rcv_req, Sys::FrontEndSendRecvType::NATIVE, // 请求信息、类型
            &Sys::handleEvent, rcehd); // 事件处理函数和事件数据

    } else if (type == ChakraNodeType::COMP_NODE) { // 如果是计算节点
        // This Compute corresponds to a reduce operation. The computation time
        // here is assumed to be trivial.

        // 这个计算节点对应的是一个 reduce（归约） 操作。在这里，计算时间被认为是可以忽略的。


        WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
        wlhd->node_id = node->id(); // 记录当前节点 ID

        uint64_t runtime = 1ul; // 默认计算时间
        if (node->runtime() != 0ul) {
            // chakra runtimes are in microseconds and we should convert it into
            // nanoseconds.
            runtime = node->runtime() * 1000;
        }

        // 注册计算事件
        stream->owner->register_event(this, EventType::General, wlhd, runtime);
    }
}

/**
 * @brief 处理所有无依赖的节点并触发执行
 */
void ChakraImpl::issue_dep_free_nodes() {
    shared_ptr<Chakra::ETFeederNode> node = et_feeder->getNextIssuableNode(); // 获取下一个可执行的节点
    while (node != nullptr) { // 遍历所有可执行的节点
        issue(node); // 执行该节点
        node = et_feeder->getNextIssuableNode(); // 获取下一个可执行节点
    }
}

// This is called when a SEND/RECV/COMP operator has completed.
// Release the Chakra node for the completed operator and issue downstream
// nodes.
/**
 * @brief 事件回调函数，处理完成的事件
 * @param event 事件类型
 * @param data 事件数据
 */
void ChakraImpl::call(EventType event, CallData* data) {
    if (data == nullptr) {
        throw runtime_error("ChakraImpl::Call does not have node id encoded "
                            "(CallData* data is null).");
    }

    WorkloadLayerHandlerData* wlhd = (WorkloadLayerHandlerData*)data;
    shared_ptr<Chakra::ETFeederNode> node =
        et_feeder->lookupNode(wlhd->node_id);

    et_feeder->freeChildrenNodes(wlhd->node_id); // 释放当前节点的所有子节点
    issue_dep_free_nodes(); // 继续调度新的可执行节点
    et_feeder->removeNode(wlhd->node_id); // 从 ETFeeder 中移除已完成的节点
    delete wlhd; // 释放内存

    if (!et_feeder->hasNodesToIssue()) { // 如果所有节点都执行完毕
        // There are no more nodes to execute, so we finish the collective
        // algorithm.
        exit();
    }
}

/**
 * @brief 运行 Chakra 集合通信算法
 * @param event 事件类型
 * @param data 事件数据
 */
void ChakraImpl::run(EventType event, CallData* data) {
    // Start executing the collective algorithm implementation by issuing the
    // root nodes.
    issue_dep_free_nodes(); // 启动无依赖的节点执行
}
