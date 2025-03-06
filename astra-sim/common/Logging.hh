#ifndef __COMMON_LOGGING_HH__
#define __COMMON_LOGGING_HH__

/**
 * @file Logging.hh
 * @brief AstraSim 的日志管理类，基于 spdlog 实现。
 * 
 * 该类提供日志记录功能，包括：
 * - 获取或创建日志器（logger）
 * - 初始化日志系统（支持从配置文件加载）
 * - 关闭日志系统，释放资源
 */

#include "spdlog/sinks/stdout_color_sinks.h"  // 控制台日志输出
#include "spdlog/spdlog.h"                    // spdlog 核心功能
#include "spdlog_setup/conf.h"                 // 允许从文件加载日志配置
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace AstraSim {

/**
 * @class LoggerFactory
 * @brief AstraSim 统一日志管理类。
 * 
 * 提供日志初始化、获取 logger、关闭日志系统的方法。
 * 该类 **禁止实例化**，所有方法均为静态方法。
 */
class LoggerFactory {
  public:
    LoggerFactory() = delete;  ///< 禁止实例化

    /**
     * @brief 获取指定名称的 logger（如果不存在则创建）。
     * @param logger_name logger 名称
     * @return 共享指针，指向 spdlog::logger 实例
     */
    static std::shared_ptr<spdlog::logger> get_logger(
        const std::string& logger_name);

    /**
     * @brief 初始化日志系统。
     * @param log_conf_path 日志配置文件路径（默认为 "empty"）
     * 
     * 如果提供了配置文件，则从文件加载日志配置，否则使用默认配置。
     */
    static void init(const std::string& log_conf_path = "empty");

    /**
     * @brief 关闭日志系统，释放资源。
     * 
     * 清理所有 logger 并释放日志组件。
     */
    static void shutdown(void);

  private:
    /**
     * @brief 初始化默认日志组件（控制台输出、文件输出等）。
     */
    static void init_default_components();

    /**
     * @brief 存储默认的日志输出组件。
     */
    static std::unordered_set<spdlog::sink_ptr> default_sinks;
};

}  // namespace AstraSim

#endif  // __COMMON_LOGGING_HH__
