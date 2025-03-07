/******************************************************************************
 * @file WorkloadLayerHandlerData.cc
 * @brief 该文件定义了 WorkloadLayerHandlerData 类的实现，用于存储
 *        工作负载（Workload）层的元数据信息，并处理相关事件。

 ******************************************************************************/
/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "WorkloadLayerHandlerData.hh"

using namespace AstraSim;

/**
 * @brief WorkloadLayerHandlerData 的构造函数
 * 
 * 该构造函数用于初始化 `node_id`，默认值为 0。
 */
WorkloadLayerHandlerData::WorkloadLayerHandlerData() {
    node_id = 0;  // 初始化 node_id
}
