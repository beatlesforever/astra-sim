/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/ChunkIdGenerator.hh" // 包含 ChunkIdGenerator 类的定义
#include <cassert> // 引入断言库，用于参数检查

using namespace AstraSimAnalytical; // 使用 AstraSimAnalytical 命名空间

/**
 * @class ChunkIdGenerator
 * @brief 该类用于为数据块（chunk）生成唯一的 ID，确保数据块在传输过程中可唯一标识。
 */
ChunkIdGenerator::ChunkIdGenerator() noexcept {
    // 初始化 chunk_id_map，存储数据块 ID 的映射关系
    chunk_id_map = {};
}

/**
 * @brief 生成唯一的发送数据块 ID
 * @param tag 消息标签（用于标识不同通信类型）
 * @param src 源节点 ID
 * @param dest 目标节点 ID
 * @param chunk_size 数据块大小
 * @return 生成的唯一数据块 ID
 */
int ChunkIdGenerator::create_send_chunk_id(
    const int tag,
    const int src,
    const int dest,
    const ChunkSize chunk_size) noexcept {
    
    // 参数合法性检查，确保所有值非负，chunk_size 必须大于 0
    assert(tag >= 0);
    assert(src >= 0);
    assert(dest >= 0);
    assert(chunk_size > 0);

    // 生成唯一键值 (tag, src, dest, chunk_size)，用于标识数据块
    const auto key = std::make_tuple(tag, src, dest, chunk_size);

    // 在 `chunk_id_map` 中查找该键是否已存在
    auto entry = chunk_id_map.find(key);

    // 如果该键不存在，则创建新的 `ChunkIdGeneratorEntry`
    if (entry == chunk_id_map.end()) {
        entry = chunk_id_map.emplace(key, ChunkIdGeneratorEntry()).first;
    }

    // 递增发送 ID，并返回新的 ID
    entry->second.increment_send_id();
    return entry->second.get_send_id();
}

/**
 * @brief 生成唯一的接收数据块 ID
 * @param tag 消息标签（用于标识不同通信类型）
 * @param src 源节点 ID
 * @param dest 目标节点 ID
 * @param chunk_size 数据块大小
 * @return 生成的唯一数据块 ID
 */
int ChunkIdGenerator::create_recv_chunk_id(
    const int tag,
    const int src,
    const int dest,
    const ChunkSize chunk_size) noexcept {
    
    // 参数合法性检查，确保所有值非负，chunk_size 必须大于 0
    assert(tag >= 0);
    assert(src >= 0);
    assert(dest >= 0);
    assert(chunk_size > 0);

    // 生成唯一键值 (tag, src, dest, chunk_size)
    const auto key = std::make_tuple(tag, src, dest, chunk_size);

    // 在 `chunk_id_map` 中查找该键是否已存在
    auto entry = chunk_id_map.find(key);

    // 如果该键不存在，则创建新的 `ChunkIdGeneratorEntry`
    if (entry == chunk_id_map.end()) {
        entry = chunk_id_map.emplace(key, ChunkIdGeneratorEntry()).first;
    }

    // 递增接收 ID，并返回新的 ID
    entry->second.increment_recv_id();
    return entry->second.get_recv_id();
}
