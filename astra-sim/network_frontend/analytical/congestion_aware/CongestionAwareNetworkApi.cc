/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "congestion_aware/CongestionAwareNetworkApi.hh" // 包含 CongestionAwareNetworkApi 类的声明，提供拥塞感知网络 API
#include <astra-network-analytical/congestion_aware/Chunk.h> // 包含 Chunk 相关定义，用于模拟数据块
#include <cassert> // 断言库，用于运行时检查

using namespace AstraSim;
using namespace AstraSimAnalyticalCongestionAware;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionAware;

/**
 * @class CongestionAwareNetworkApi
 * @brief 该类继承自 `CommonNetworkApi`，用于实现拥塞感知的网络 API，能够基于拓扑结构进行路径计算和数据传输。
 */

/** 
 * @brief 静态变量定义 
 * `topology` 用于存储当前网络拓扑信息，该变量在所有 `CongestionAwareNetworkApi` 实例间共享。
 */
std::shared_ptr<Topology> CongestionAwareNetworkApi::topology;

/**
 * @brief 设置网络拓扑
 * @param topology_ptr 指向 `Topology` 对象的共享指针
 */
void CongestionAwareNetworkApi::set_topology(
    std::shared_ptr<Topology> topology_ptr) noexcept {
    assert(topology_ptr != nullptr); // 确保拓扑对象指针不为空

    // 赋值给静态成员变量
    CongestionAwareNetworkApi::topology = std::move(topology_ptr);

    // 设置基于拓扑的网络参数
    CongestionAwareNetworkApi::dims_count =
        CongestionAwareNetworkApi::topology->get_dims_count(); // 获取维度数
    CongestionAwareNetworkApi::bandwidth_per_dim =
        CongestionAwareNetworkApi::topology->get_bandwidth_per_dim(); // 获取每个维度的带宽
}

/**
 * @brief 构造函数
 * @param rank 当前节点的 ID
 */
CongestionAwareNetworkApi::CongestionAwareNetworkApi(const int rank) noexcept
    : CommonNetworkApi(rank) {
    assert(rank >= 0); // 确保 rank 合法
}

/**
 * @brief 发送数据
 * @param buffer 存储发送数据的缓冲区
 * @param count 数据块大小
 * @param type 数据类型
 * @param dst 目标节点 ID
 * @param tag 消息标签
 * @param request 传输请求指针
 * @param msg_handler 发送完成后的回调函数
 * @param fun_arg 传递给回调函数的参数
 * @return 操作状态（0 表示成功）
 */
int CongestionAwareNetworkApi::sim_send(void* const buffer,
                                        const uint64_t count,
                                        const int type,
                                        const int dst,
                                        const int tag,
                                        sim_request* const request,
                                        void (*msg_handler)(void*),
                                        void* const fun_arg) {
    // 获取当前节点 ID（源节点）
    const auto src = sim_comm_get_rank();

    // 生成唯一的发送数据块 ID
    const auto chunk_id =
        CongestionAwareNetworkApi::chunk_id_generator.create_send_chunk_id(
            tag, src, dst, count);

    // 在回调追踪器中查找该数据块的回调条目
    const auto entry =
        callback_tracker.search_entry(tag, src, dst, count, chunk_id);
    if (entry.has_value()) {
        // 如果接收操作已经被调用，则直接注册发送回调
        entry.value()->register_send_callback(msg_handler, fun_arg);
    } else {
        // 如果接收操作尚未调用，则创建新的条目，并注册发送回调
        auto* const new_entry =
            callback_tracker.create_new_entry(tag, src, dst, count, chunk_id);
        new_entry->register_send_callback(msg_handler, fun_arg);
    }

    // 创建数据块传输参数
    auto chunk_arrival_arg = std::tuple(tag, src, dst, count, chunk_id);
    auto arg = std::make_unique<decltype(chunk_arrival_arg)>(chunk_arrival_arg);
    const auto arg_ptr = static_cast<void*>(arg.release()); // 转换为 void* 以便传递

    // 计算从 `src` 到 `dst` 的路由路径
    const auto route = topology->route(src, dst);

    // 创建 `Chunk` 对象，表示该数据块的传输任务
    auto chunk = std::make_unique<Chunk>(
        count, route, CongestionAwareNetworkApi::process_chunk_arrival,
        arg_ptr);

    // 通过拓扑进行实际传输
    topology->send(std::move(chunk));

    return 0; // 返回成功状态
}
