/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/collective/Algorithm.hh"

using namespace AstraSim;

/**
 * @brief 构造函数，初始化 Algorithm 对象
 */
Algorithm::Algorithm() {
    enabled = true;  // 默认启用该算法
}

/**
 * @brief 初始化算法对象，并绑定数据流
 * @param stream 指向 BaseStream 对象的指针
 */
void Algorithm::init(BaseStream* stream) {
    this->stream = stream;  // 关联数据流
}

/**
 * @brief 事件回调函数，供子类重写
 * @param event 事件类型
 * @param data 事件相关数据
 */
void Algorithm::call(EventType event, CallData* data) {
    // 默认情况下不执行任何操作，由具体算法实现
}

/**
 * @brief 退出当前算法并通知下一个虚拟网络继续
 */
void Algorithm::exit() {
    // 触发下一个 VNet（虚拟网络）基线的执行
    stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
}
