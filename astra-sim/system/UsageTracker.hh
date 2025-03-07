/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __USAGE_TRACKER_HH__
#define __USAGE_TRACKER_HH__

#include <cstdint>

#include "astra-sim/system/CSVWriter.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/Usage.hh"

namespace AstraSim {

/**
 * @class UsageTracker
 * @brief 该类用于跟踪资源使用情况，并提供数据报告功能。
 * 
 * 该类可以记录资源使用的变化，例如 CPU、GPU 或网络的负载情况。
 * 它维护一个 `usage` 列表，其中每个 `Usage` 记录一个时间段内的使用状态。
 */
class UsageTracker {
  public:
    /**
     * @brief 构造函数
     * @param levels 资源使用的等级数（越高表示负载越大）
     */
    UsageTracker(int levels);

    /**
     * @brief 增加资源使用等级
     * 
     * 如果当前使用等级小于最大等级，则增加等级并记录时间戳。
     */
    void increase_usage();

    /**
     * @brief 降低资源使用等级
     * 
     * 如果当前使用等级大于 0，则减少等级并记录时间戳。
     */
    void decrease_usage();

    /**
     * @brief 直接设置资源使用等级
     * @param level 要设置的等级
     * 
     * 如果当前等级与指定等级不同，则记录变化并更新时间戳。
     */
    void set_usage(int level);

    /**
     * @brief 生成资源使用报告，并写入 CSV 文件
     * @param writer CSV 文件写入器
     * @param offset 数据列偏移量
     */
    void report(CSVWriter* writer, int offset);

    /**
     * @brief 计算每个周期的资源使用百分比
     * @param cycles 计算周期（以时钟周期计）
     * @return 返回一个列表，每个元素为 `(时间戳, 资源使用百分比)`
     */
    std::list<std::pair<uint64_t, double>> report_percentage(uint64_t cycles);

    int levels;                 ///< 资源使用的最大等级
    int current_level;           ///< 当前使用等级
    Tick last_tick;              ///< 上一次使用等级变化的时间戳
    std::list<Usage> usage;      ///< 记录资源使用情况的时间片段列表
};

}  // namespace AstraSim

#endif /* __USAGE_TRACKER_HH__ */
