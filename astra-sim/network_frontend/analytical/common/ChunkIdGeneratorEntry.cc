/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/ChunkIdGeneratorEntry.hh" // 包含 ChunkIdGeneratorEntry 类的定义
#include <cassert> // 断言库，用于运行时检查

using namespace AstraSimAnalytical; // 使用 AstraSimAnalytical 命名空间

/**
 * @class ChunkIdGeneratorEntry
 * @brief 该类用于管理数据块的唯一 ID，分别为发送 ID 和接收 ID，并提供 ID 递增和获取功能。
 */
ChunkIdGeneratorEntry::ChunkIdGeneratorEntry() noexcept
    : send_id(-1),  // 发送 ID 初始值设为 -1，表示尚未分配
      recv_id(-1) {} // 接收 ID 初始值设为 -1，表示尚未分配

/**
 * @brief 获取当前的发送 ID
 * @return 当前的发送 ID
 */
int ChunkIdGeneratorEntry::get_send_id() const noexcept {
    assert(send_id >= 0); // 确保发送 ID 已被正确分配
    return send_id;
}

/**
 * @brief 获取当前的接收 ID
 * @return 当前的接收 ID
 */
int ChunkIdGeneratorEntry::get_recv_id() const noexcept {
    assert(recv_id >= 0); // 确保接收 ID 已被正确分配
    return recv_id;
}

/**
 * @brief 递增发送 ID
 */
void ChunkIdGeneratorEntry::increment_send_id() noexcept {
    send_id++; // 发送 ID 递增，确保每个发送数据块有唯一的标识
}

/**
 * @brief 递增接收 ID
 */
void ChunkIdGeneratorEntry::increment_recv_id() noexcept {
    recv_id++; // 接收 ID 递增，确保每个接收数据块有唯一的标识
}
