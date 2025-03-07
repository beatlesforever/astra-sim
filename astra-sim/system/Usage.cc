/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/Usage.hh"

using namespace AstraSim;

/**
 * @brief Usage 构造函数
 * @param level 资源使用的等级（如 CPU/GPU 负载）
 * @param start 资源使用的起始时间戳
 * @param end 资源使用的结束时间戳
 * 
 * 该构造函数用于初始化 `Usage` 对象，存储某个时间段内的资源使用情况。
 */
Usage::Usage(int level, uint64_t start, uint64_t end) {
    this->level = level;  // 设置资源使用等级
    this->start = start;  // 设置起始时间
    this->end = end;      // 设置结束时间
}
