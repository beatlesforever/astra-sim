/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/CallbackTracker.hh" // 包含 CallbackTracker 类的声明
#include <cassert> // 断言库，确保运行时检查

using namespace AstraSimAnalytical; // 使用 AstraSimAnalytical 命名空间

/**
 * @class CallbackTracker
 * @brief 用于跟踪和管理回调条目，提供查询、创建和删除功能。
 */
CallbackTracker::CallbackTracker() noexcept {
    // 初始化 tracker，存储回调条目的映射
    tracker = {};
}

/**
 * @brief 搜索指定的回调条目
 * @param tag 消息标签
 * @param src 源节点 ID
 * @param dest 目标节点 ID
 * @param chunk_size 数据块大小
 * @param chunk_id 数据块 ID
 * @return 若找到，则返回指向 `CallbackTrackerEntry` 的指针，否则返回 `std::nullopt`
 */
std::optional<CallbackTrackerEntry*> CallbackTracker::search_entry(
    const int tag,
    const int src,
    const int dest,
    const ChunkSize chunk_size,
    const int chunk_id) noexcept {
    
    // 参数合法性检查，确保所有值非负，chunk_size 大于 0
    assert(tag >= 0);
    assert(src >= 0);
    assert(dest >= 0);
    assert(chunk_size > 0);
    assert(chunk_id >= 0);

    // 生成唯一键值 (tag, src, dest, chunk_size, chunk_id)
    const auto key = std::make_tuple(tag, src, dest, chunk_size, chunk_id);
    
    // 在 tracker 中查找对应的回调条目
    const auto entry = tracker.find(key);

    // 若未找到条目，返回空值
    if (entry == tracker.end()) {
        return std::nullopt;
    }

    // 返回找到的条目的指针
    return &(entry->second);
}

/**
 * @brief 创建新的回调条目
 * @param tag 消息标签
 * @param src 源节点 ID
 * @param dest 目标节点 ID
 * @param chunk_size 数据块大小
 * @param chunk_id 数据块 ID
 * @return 返回指向新创建的 `CallbackTrackerEntry` 的指针
 */
CallbackTrackerEntry* CallbackTracker::create_new_entry(
    const int tag,
    const int src,
    const int dest,
    const ChunkSize chunk_size,
    const int chunk_id) noexcept {
    
    // 参数合法性检查
    assert(tag >= 0);
    assert(src >= 0);
    assert(dest >= 0);
    assert(chunk_size > 0);
    assert(chunk_id >= 0);

    // 生成唯一键值 (tag, src, dest, chunk_size, chunk_id)
    const auto key = std::make_tuple(tag, src, dest, chunk_size, chunk_id);

    // 在 `tracker` 中创建新的空 `CallbackTrackerEntry`
    const auto entry = tracker.emplace(key, CallbackTrackerEntry()).first;

    // 返回指向新创建条目的指针
    return &(entry->second);
}

/**
 * @brief 删除指定的回调条目
 * @param tag 消息标签
 * @param src 源节点 ID
 * @param dest 目标节点 ID
 * @param chunk_size 数据块大小
 * @param chunk_id 数据块 ID
 */
void CallbackTracker::pop_entry(const int tag,
                                const int src,
                                const int dest,
                                const ChunkSize chunk_size,
                                const int chunk_id) noexcept {
    
    // 参数合法性检查
    assert(tag >= 0);
    assert(src >= 0);
    assert(dest >= 0);
    assert(chunk_size > 0);
    assert(chunk_id >= 0);

    // 生成唯一键值 (tag, src, dest, chunk_size, chunk_id)
    const auto key = std::make_tuple(tag, src, dest, chunk_size, chunk_id);

    // 在 tracker 中查找该回调条目
    const auto entry = tracker.find(key);
    assert(entry != tracker.end());  // 确保条目存在，否则触发断言失败

    // 从 tracker 中删除该条目
    tracker.erase(entry);
}
