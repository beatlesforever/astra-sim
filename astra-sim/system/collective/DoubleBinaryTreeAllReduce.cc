/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/DoubleBinaryTreeAllReduce.hh"

#include "astra-sim/system/PacketBundle.hh" // 负责数据包的封装和发送
#include "astra-sim/system/RecvPacketEventHandlerData.hh" // 处理接收数据包的事件数据

using namespace AstraSim;

/**
 * @brief DoubleBinaryTreeAllReduce 构造函数
 * @param id 当前进程的唯一标识
 * @param tree 二叉树结构，表示通信拓扑
 * @param data_size 需要 All-Reduce 处理的数据大小
 */
DoubleBinaryTreeAllReduce::DoubleBinaryTreeAllReduce(int id,
                                                     BinaryTree* tree,
                                                     uint64_t data_size)
    : Algorithm() { // 调用基类 Algorithm 的构造函数
    this->id = id; // 存储进程 ID
    this->logical_topo = tree; // 存储二叉树拓扑
    this->data_size = data_size; // 存储通信数据大小
    this->state = State::Begin; // 初始化状态为 Begin
    this->reductions = 0; // 归约次数设为 0
    this->parent = tree->get_parent_id(id); // 获取当前节点的父节点 ID
    this->left_child = tree->get_left_child_id(id); // 获取左子节点 ID
    this->right_child = tree->get_right_child_id(id); // 获取右子节点 ID
    this->type = tree->get_node_type(id); // 获取当前节点的类型（叶子、中间节点或根）
    this->final_data_size = data_size; // 最终数据大小
    this->comType = ComType::All_Reduce; // 通信类型设置为 All-Reduce
    this->name = Name::DoubleBinaryTree; // 算法名称
}

/**
 * @brief 运行双二叉树 All-Reduce 算法
 * @param event 事件类型（如数据包接收、发送等）
 * @param data 事件相关数据
 */
void DoubleBinaryTreeAllReduce::run(EventType event, CallData* data) {
    // 叶子节点开始发送数据

    if (state == State::Begin && type == BinaryTree::Type::Leaf) {  // leaf.1
        (new PacketBundle(stream->owner, stream, false, false, data_size,
                          MemBus::Transmition::Usual))
            ->send_to_MA();
        state = State::SendingDataToParent; // 更新状态：发送数据给父节点

    } else if (state == State::SendingDataToParent &&
               type == BinaryTree::Type::Leaf) {  // leaf.3
        // sending
        // 发送数据到父节点
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = parent;
        snd_req.tag = stream->stream_id;
        snd_req.reqType = UINT8;
        snd_req.vnet = this->stream->current_queue_id;
        stream->owner->front_end_sim_send(0, Sys::dummy_data, data_size, UINT8,
                                          parent, stream->stream_id, &snd_req,
                                          Sys::FrontEndSendRecvType::COLLECTIVE,
                                          &Sys::handleEvent, nullptr);
        // receiving
        sim_request rcv_req;
        rcv_req.vnet = this->stream->current_queue_id;
        RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
            stream, stream->owner->id, EventType::PacketReceived,
            stream->current_queue_id, stream->stream_id);
        stream->owner->front_end_sim_recv(0, Sys::dummy_data, data_size, UINT8,
                                          parent, stream->stream_id, &rcv_req,
                                          Sys::FrontEndSendRecvType::COLLECTIVE,
                                          &Sys::handleEvent, ehd);
        state = State::WaitingDataFromParent;

    } else if (state == State::WaitingDataFromParent &&
               type == BinaryTree::Type::Leaf) {  // leaf.4
        (new PacketBundle(stream->owner, stream, false, false, data_size,
                          MemBus::Transmition::Usual))
            ->send_to_NPU();
        state = State::End;

    } else if (state == State::End &&
               type == BinaryTree::Type::Leaf) {  // leaf.5
        exit();

    } else if (state == State::Begin &&
               type == BinaryTree::Type::Intermediate) {  // int.1
        sim_request rcv_req;
        rcv_req.vnet = this->stream->current_queue_id;
        RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
            stream, stream->owner->id, EventType::PacketReceived,
            stream->current_queue_id, stream->stream_id);
        stream->owner->front_end_sim_recv(
            0, Sys::dummy_data, data_size, UINT8, left_child, stream->stream_id,
            &rcv_req, Sys::FrontEndSendRecvType::COLLECTIVE, &Sys::handleEvent,
            ehd);
        sim_request rcv_req2;
        rcv_req2.vnet = this->stream->current_queue_id;
        RecvPacketEventHandlerData* ehd2 = new RecvPacketEventHandlerData(
            stream, stream->owner->id, EventType::PacketReceived,
            stream->current_queue_id, stream->stream_id);
        stream->owner->front_end_sim_recv(
            0, Sys::dummy_data, data_size, UINT8, right_child,
            stream->stream_id, &rcv_req2, Sys::FrontEndSendRecvType::COLLECTIVE,
            &Sys::handleEvent, ehd2);
        state = State::WaitingForTwoChildData;

    } else if (state == State::WaitingForTwoChildData &&
               type == BinaryTree::Type::Intermediate &&
               event == EventType::PacketReceived) {  // int.2
        (new PacketBundle(stream->owner, stream, true, false, data_size,
                          MemBus::Transmition::Usual))
            ->send_to_NPU();
        state = State::WaitingForOneChildData;

    } else if (state == State::WaitingForOneChildData &&
               type == BinaryTree::Type::Intermediate &&
               event == EventType::PacketReceived) {  // int.3
        (new PacketBundle(stream->owner, stream, true, true, data_size,
                          MemBus::Transmition::Usual))
            ->send_to_NPU();
        state = State::SendingDataToParent;

    } else if (reductions < 1 && type == BinaryTree::Type::Intermediate &&
               event == EventType::General) {  // int.4
        reductions++;

    } else if (state == State::SendingDataToParent &&
               type == BinaryTree::Type::Intermediate) {  // int.5
        // sending
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = parent;
        snd_req.tag = stream->stream_id;
        snd_req.reqType = UINT8;
        snd_req.vnet = this->stream->current_queue_id;
        stream->owner->front_end_sim_send(0, Sys::dummy_data, data_size, UINT8,
                                          parent, stream->stream_id, &snd_req,
                                          Sys::FrontEndSendRecvType::COLLECTIVE,
                                          &Sys::handleEvent, nullptr);
        // receiving
        sim_request rcv_req;
        rcv_req.vnet = this->stream->current_queue_id;
        RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
            stream, stream->owner->id, EventType::PacketReceived,
            stream->current_queue_id, stream->stream_id);
        stream->owner->front_end_sim_recv(0, Sys::dummy_data, data_size, UINT8,
                                          parent, stream->stream_id, &rcv_req,
                                          Sys::FrontEndSendRecvType::COLLECTIVE,
                                          &Sys::handleEvent, ehd);
        state = State::WaitingDataFromParent;

    } else if (state == State::WaitingDataFromParent &&
               type == BinaryTree::Type::Intermediate &&
               event == EventType::PacketReceived) {  // int.6
        (new PacketBundle(stream->owner, stream, true, true, data_size,
                          MemBus::Transmition::Usual))
            ->send_to_NPU();
        state = State::SendingDataToChilds;

    } else if (state == State::SendingDataToChilds &&
               type == BinaryTree::Type::Intermediate) {
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = left_child;
        snd_req.tag = stream->stream_id;
        snd_req.reqType = UINT8;
        snd_req.vnet = this->stream->current_queue_id;
        stream->owner->front_end_sim_send(
            0, Sys::dummy_data, data_size, UINT8, left_child, stream->stream_id,
            &snd_req, Sys::FrontEndSendRecvType::COLLECTIVE, &Sys::handleEvent,
            nullptr);
        sim_request snd_req2;
        snd_req2.srcRank = stream->owner->id;
        snd_req2.dstRank = left_child;
        snd_req2.tag = stream->stream_id;
        snd_req2.reqType = UINT8;
        snd_req2.vnet = this->stream->current_queue_id;
        stream->owner->front_end_sim_send(
            0, Sys::dummy_data, data_size, UINT8, right_child,
            stream->stream_id, &snd_req2, Sys::FrontEndSendRecvType::COLLECTIVE,
            &Sys::handleEvent, nullptr);
        exit();
        return;

    } else if (state == State::Begin &&
               type == BinaryTree::Type::Root) {  // root.1
        int only_child_id = left_child >= 0 ? left_child : right_child;
        sim_request rcv_req;
        rcv_req.vnet = this->stream->current_queue_id;
        RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
            stream, stream->owner->id, EventType::PacketReceived,
            stream->current_queue_id, stream->stream_id);
        stream->owner->front_end_sim_recv(
            0, Sys::dummy_data, data_size, UINT8, only_child_id,
            stream->stream_id, &rcv_req, Sys::FrontEndSendRecvType::COLLECTIVE,
            &Sys::handleEvent, ehd);
        state = State::WaitingForOneChildData;

    } else if (state == State::WaitingForOneChildData &&
               type == BinaryTree::Type::Root) {  // root.2
        (new PacketBundle(stream->owner, stream, true, true, data_size,
                          MemBus::Transmition::Usual))
            ->send_to_NPU();
        state = State::SendingDataToChilds;
        return;

    } else if (state == State::SendingDataToChilds &&
               type == BinaryTree::Type::Root) {  // root.2
        int only_child_id = left_child >= 0 ? left_child : right_child;
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = only_child_id;
        snd_req.tag = stream->stream_id;
        snd_req.reqType = UINT8;
        snd_req.vnet = this->stream->current_queue_id;
        stream->owner->front_end_sim_send(
            0, Sys::dummy_data, data_size, UINT8, only_child_id,
            stream->stream_id, &snd_req, Sys::FrontEndSendRecvType::COLLECTIVE,
            &Sys::handleEvent, nullptr);
        exit();
        return;
    }
}
