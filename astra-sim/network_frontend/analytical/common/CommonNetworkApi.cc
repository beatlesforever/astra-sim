/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/CommonNetworkApi.hh" // 包含 CommonNetworkApi 类的定义，提供网络通信 API
#include <cassert> // 断言库，用于参数检查

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;

/**
 * @class CommonNetworkApi
 * @brief 该类提供 Astra-sim 中的通用网络 API，实现事件调度、数据接收等功能。
 */

/** 
 * @brief 静态变量定义 
 * 这些变量在所有 `CommonNetworkApi` 实例中共享
 */
std::shared_ptr<EventQueue> CommonNetworkApi::event_queue = nullptr; // 事件队列指针
ChunkIdGenerator CommonNetworkApi::chunk_id_generator = {}; // 用于生成数据块唯一 ID
CallbackTracker CommonNetworkApi::callback_tracker = {}; // 追踪回调事件
int CommonNetworkApi::dims_count = -1; // 维度数，初始化为 -1
std::vector<Bandwidth> CommonNetworkApi::bandwidth_per_dim = {}; // 每个维度的带宽存储

/**
 * @brief 设置全局事件队列
 * @param event_queue_ptr 指向 EventQueue 的共享指针
 */
void CommonNetworkApi::set_event_queue(
    std::shared_ptr<EventQueue> event_queue_ptr) noexcept {
    assert(event_queue_ptr != nullptr); // 确保 event_queue_ptr 不为空

    // 赋值给静态成员变量
    CommonNetworkApi::event_queue = std::move(event_queue_ptr);
}

/**
 * @brief 获取回调追踪器
 * @return 返回全局 CallbackTracker 引用
 */
CallbackTracker& CommonNetworkApi::get_callback_tracker() noexcept {
    return callback_tracker;
}

/**
 * @brief 处理数据块到达事件
 * @param args 指向数据块元数据的指针
 */
void CommonNetworkApi::process_chunk_arrival(void* args) noexcept {
    assert(args != nullptr); // 确保参数不为空

    // 解析数据块信息
    auto* const data =
        static_cast<std::tuple<int, int, int, uint64_t, int>*>(args);
    const auto [tag, src, dest, count, chunk_id] = *data;

    // 获取回调追踪器
    auto& tracker = CommonNetworkApi::get_callback_tracker();
    const auto entry = tracker.search_entry(tag, src, dest, count, chunk_id);
    assert(entry.has_value());  // 确保找到对应的回调条目

    // 如果发送和接收回调都已注册，执行回调并删除条目
    if (entry.value()->both_callbacks_registered()) {
        entry.value()->invoke_send_handler();
        entry.value()->invoke_recv_handler();

        // 删除该回调条目
        tracker.pop_entry(tag, src, dest, count, chunk_id);
    } else {
        // 仅执行发送回调（接收回调未准备好）
        entry.value()->invoke_send_handler();

        // 标记传输完成
        // 当 `sim_recv()` 被调用时，接收回调将立即触发
        entry.value()->set_transmission_finished();
    }
}

/**
 * @brief CommonNetworkApi 构造函数
 * @param rank 该节点的排名 ID
 */
CommonNetworkApi::CommonNetworkApi(const int rank) noexcept
    : AstraNetworkAPI(rank) {
    assert(rank >= 0); // 确保 rank 合法
}

/**
 * @brief 获取当前的仿真时间
 * @return 当前时间，以 ASTRA-sim 格式表示
 */
timespec_t CommonNetworkApi::sim_get_time() {
    // 获取当前事件队列时间
    const auto current_time = event_queue->get_current_time();

    // 转换时间格式并返回
    const auto astra_sim_time = static_cast<double>(current_time);
    return {NS, astra_sim_time};
}

/**
 * @brief 调度事件
 * @param delta 事件触发的时间间隔
 * @param fun_ptr 事件回调函数指针
 * @param fun_arg 事件回调函数参数
 */
void CommonNetworkApi::sim_schedule(const timespec_t delta,
                                    void (*fun_ptr)(void*),
                                    void* const fun_arg) {
    assert(delta.time_res == NS); // 确保时间单位是纳秒
    assert(fun_ptr != nullptr); // 确保函数指针有效

    // 计算事件的绝对触发时间
    const auto current_time = sim_get_time();
    const auto event_time = current_time.time_val + delta.time_val;
    const auto event_time_ns = static_cast<EventTime>(event_time);

    // 确保事件时间不早于当前时间
    assert(event_time_ns >= event_queue->get_current_time());

    // 将事件加入事件队列
    event_queue->schedule_event(event_time_ns, fun_ptr, fun_arg);
}

/**
 * @brief 处理数据接收
 * @param buffer 存储接收数据的缓冲区
 * @param count 数据大小
 * @param type 数据类型
 * @param src 源节点
 * @param tag 消息标签
 * @param request 传输请求
 * @param msg_handler 消息处理回调函数
 * @param fun_arg 消息回调参数
 * @return 操作状态
 */
int CommonNetworkApi::sim_recv(void* const buffer,
                               const uint64_t count,
                               const int type,
                               const int src,
                               const int tag,
                               sim_request* const request,
                               void (*msg_handler)(void*),
                               void* const fun_arg) {
    // 获取当前节点 ID
    const auto dst = sim_comm_get_rank();

    // 生成唯一的接收 chunk ID
    const auto chunk_id =
        CommonNetworkApi::chunk_id_generator.create_recv_chunk_id(tag, src, dst,
                                                                  count);

    // 查询是否已存在回调条目
    auto entry = callback_tracker.search_entry(tag, src, dst, count, chunk_id);
    if (entry.has_value()) {
        // 若发送已调用，判断传输是否完成
        if (entry.value()->is_transmission_finished()) {
            // 传输已完成，立即触发回调
            callback_tracker.pop_entry(tag, src, dst, count, chunk_id);

            // 调度回调
            const auto delta = timespec_t{NS, 0};
            sim_schedule(delta, msg_handler, fun_arg);
        } else {
            // 传输未完成，注册接收回调
            entry.value()->register_recv_callback(msg_handler, fun_arg);
        }
    } else {
        // 发送未调用，创建新条目并注册回调
        auto* const new_entry =
            callback_tracker.create_new_entry(tag, src, dst, count, chunk_id);
        new_entry->register_recv_callback(msg_handler, fun_arg);
    }

    return 0; // 返回成功状态
}

/**
 * @brief 获取指定维度的带宽
 * @param dim 维度索引
 * @return 该维度的带宽值
 */
double CommonNetworkApi::get_BW_at_dimension(const int dim) {
    assert(0 <= dim && dim < dims_count); // 确保维度索引合法

    // 返回指定维度的带宽
    return bandwidth_per_dim[dim];
}
