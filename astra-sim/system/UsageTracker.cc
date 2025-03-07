/******************************************************************************
 * @file UsageTracker.cc
 * @brief 该文件实现了 UsageTracker 类，该类用于跟踪系统的资源使用情况，
 *        并提供数据统计和报告功能。
 * 
 ******************************************************************************/

 #include "astra-sim/system/UsageTracker.hh"  // 资源使用跟踪器的头文件
 #include "astra-sim/system/Sys.hh"           // 包含系统时间相关操作
 
 using namespace AstraSim;  // 使用 AstraSim 命名空间，避免冗长的前缀
 
 /**
  * @brief UsageTracker 构造函数
  * @param levels 资源使用的最大等级（表示最大负载程度）
  * 
  * 初始化 `levels`（最大使用等级），`current_level`（当前等级）设为 0，
  * `last_tick`（上次使用变化的时间戳）设为 0。
  */
 UsageTracker::UsageTracker(int levels) {
     this->levels = levels;
     this->current_level = 0;
     this->last_tick = 0;
 }
 
 /**
  * @brief 增加资源使用等级
  * 
  * 如果当前使用等级小于 `levels - 1`，则增加等级，并记录时间戳。
  */
 void UsageTracker::increase_usage() {
     if (current_level < levels - 1) {  // 确保不会超过最大使用等级
         Usage u(current_level, last_tick, Sys::boostedTick());  // 记录当前使用情况
         usage.push_back(u);  // 将该使用记录存入 usage 列表
         current_level++;  // 增加当前使用等级
         last_tick = Sys::boostedTick();  // 更新最新的时间戳
     }
 }
 
 /**
  * @brief 降低资源使用等级
  * 
  * 如果当前使用等级大于 0，则减少等级，并记录时间戳。
  */
 void UsageTracker::decrease_usage() {
     if (current_level > 0) {  // 确保不会低于最低等级
         Usage u(current_level, last_tick, Sys::boostedTick());  // 记录当前使用情况
         usage.push_back(u);  // 将该使用记录存入 usage 列表
         current_level--;  // 减少当前使用等级
         last_tick = Sys::boostedTick();  // 更新最新的时间戳
     }
 }
 
 /**
  * @brief 直接设置资源使用等级
  * @param level 要设置的等级
  * 
  * 只有当 `level` 与 `current_level` 不同时才进行更新。
  */
 void UsageTracker::set_usage(int level) {
     if (current_level != level) {  // 只有当前等级不同才进行更新
         Usage u(current_level, last_tick, Sys::boostedTick());  // 记录当前使用情况
         usage.push_back(u);  // 将该使用记录存入 usage 列表
         current_level = level;  // 更新当前使用等级
         last_tick = Sys::boostedTick();  // 更新最新的时间戳
     }
 }
 
 /**
  * @brief 生成资源使用情况的 CSV 报告
  * @param writer CSVWriter 对象，用于写入数据
  * @param offset 数据列的偏移量
  */
 void UsageTracker::report(CSVWriter* writer, int offset) {
     uint64_t col = offset * 3;  // 计算列偏移量
     uint64_t row = 1;  // 从第一行开始写入
     for (auto a : usage) {
         writer->write_cell(row, col, std::to_string(a.start));  // 写入起始时间
         writer->write_cell(row++, col + 1, std::to_string(a.level));  // 写入使用等级
     }
     return;
 }
 
 /**
  * @brief 计算资源使用的百分比
  * @param cycles 计算周期（以时钟周期计）
  * @return 返回一个列表，每个元素为 `(时间戳, 资源使用百分比)`
  */
 std::list<std::pair<uint64_t, double>> UsageTracker::report_percentage(uint64_t cycles) {
     decrease_usage();  // 先减少使用，以确保所有的使用数据正确存储
     increase_usage();  // 重新增加，以确保数据完整
 
     Tick total_activity_possible = (this->levels - 1) * cycles;  // 计算理论上的最大活动值
     std::list<Usage>::iterator usage_pointer = this->usage.begin();  // 迭代器指向 `usage` 列表的起始位置
     Tick current_activity = 0;  // 当前活动量
     Tick period_start = 0;  // 计算周期起点
     Tick period_end = cycles;  // 计算周期终点
     std::list<std::pair<uint64_t, double>> result;  // 结果存储列表
 
     // 遍历 usage 列表，计算每个周期内的资源使用百分比
     while (usage_pointer != this->usage.end()) {
         Usage current_usage = *usage_pointer;  // 获取当前 usage 记录
         uint64_t begin = std::max(static_cast<uint64_t>(period_start), current_usage.start);  // 计算起始时间
         uint64_t end = std::min(static_cast<uint64_t>(period_end), current_usage.end);  // 计算结束时间
         assert(begin <= end);  // 确保时间顺序正确
 
         // 计算当前时间段的活动量
         current_activity += ((end - begin) * current_usage.level);
 
         // 如果当前 usage 记录已经覆盖了当前周期，则计算百分比
         if (current_usage.end >= period_end) {
             result.push_back(std::make_pair(
                 (uint64_t)period_end,
                 (((double)current_activity) / total_activity_possible) * 100));  // 计算使用百分比
             period_start += cycles;  // 更新下一个周期的起始点
             period_end += cycles;  // 更新下一个周期的终点
             current_activity = 0;  // 重置当前活动量
         } else {
             std::advance(usage_pointer, 1);  // 继续遍历 usage 列表
         }
     }
     return result;  // 返回计算结果
 }
 