/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/CallbackTrackerEntry.hh" // 引入 CallbackTrackerEntry 类的定义
#include <cassert> // 断言库，确保运行时条件检查

// 使用 NetworkAnalytical 和 AstraSimAnalytical 命名空间
using namespace NetworkAnalytical;
using namespace AstraSimAnalytical;

/**
 * @class CallbackTrackerEntry
 * @brief 该类用于存储和管理回调事件，包括发送和接收回调的注册、执行及状态管理。
 */
CallbackTrackerEntry::CallbackTrackerEntry() noexcept
    : send_event(std::nullopt),  // 发送回调初始为空
      recv_event(std::nullopt),  // 接收回调初始为空
      transmission_finished(false) {}  // 传输状态初始为未完成

/**
 * @brief 注册发送回调
 * @param callback 发送事件的回调函数
 * @param arg 回调参数
 */
void CallbackTrackerEntry::register_send_callback(
    const Callback callback, const CallbackArg arg) noexcept {
    assert(!send_event.has_value()); // 断言发送回调尚未注册

    // 创建并注册发送回调事件
    const auto event = Event(callback, arg);
    send_event = event;
}

/**
 * @brief 注册接收回调
 * @param callback 接收事件的回调函数
 * @param arg 回调参数
 */
void CallbackTrackerEntry::register_recv_callback(
    const Callback callback, const CallbackArg arg) noexcept {
    assert(!recv_event.has_value()); // 断言接收回调尚未注册

    // 创建并注册接收回调事件
    const auto event = Event(callback, arg);
    recv_event = event;
}

/**
 * @brief 检查当前数据传输是否完成
 * @return 传输是否已完成（true：已完成，false：未完成）
 */
bool CallbackTrackerEntry::is_transmission_finished() const noexcept {
    return transmission_finished;
}

/**
 * @brief 标记数据传输完成
 */
void CallbackTrackerEntry::set_transmission_finished() noexcept {
    transmission_finished = true;
}

/**
 * @brief 检查是否已注册了发送和接收回调
 * @return 如果两种回调都注册了，则返回 true，否则返回 false
 */
bool CallbackTrackerEntry::both_callbacks_registered() const noexcept {
    return (send_event.has_value() && recv_event.has_value());
}

/**
 * @brief 调用发送回调处理程序
 */
void CallbackTrackerEntry::invoke_send_handler() noexcept {
    assert(send_event.has_value()); // 确保发送回调已注册

    // 调用发送事件的回调函数
    send_event.value().invoke_event();
}

/**
 * @brief 调用接收回调处理程序
 */
void CallbackTrackerEntry::invoke_recv_handler() noexcept {
    assert(recv_event.has_value()); // 确保接收回调已注册

    // 调用接收事件的回调函数
    recv_event.value().invoke_event();
}
