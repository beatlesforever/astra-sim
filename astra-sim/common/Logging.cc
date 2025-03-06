#include "astra-sim/common/Logging.hh"  // 引入 Logging.hh 头文件

namespace AstraSim {  // 进入 AstraSim 命名空间

/**
 * @brief 存储默认的日志输出（sinks）。
 * 
 * `spdlog::sink_ptr` 是指向日志输出设备的共享指针（如控制台、文件等）。
 * `default_sinks` 这个 `unordered_set` 存储了所有默认的日志输出方式，
 * 这些输出方式在日志初始化时进行配置，并应用于所有 logger。
 */
std::unordered_set<spdlog::sink_ptr> LoggerFactory::default_sinks;

/**
 * @brief 获取（或创建）指定名称的 logger。
 * 
 * @param logger_name 要获取的 logger 名称。
 * @return 返回一个 `spdlog::logger` 的共享指针。
 * 
 * 该方法首先检查 `spdlog` 是否已存在指定名称的 logger：
 * - **如果已存在**，直接返回该 logger。
 * - **如果不存在**，则创建一个 **异步的空日志（null_sink_mt）**，并设置日志级别。
 * - 如果 `ENABLE_DEFAULT_SINK_FOR_OTHER_LOGGERS` 设为 `true`，
 *   则会为该 logger **附加默认的日志输出**（如终端、文件）。
 */
std::shared_ptr<spdlog::logger> LoggerFactory::get_logger(
    const std::string& logger_name) {
    constexpr bool ENABLE_DEFAULT_SINK_FOR_OTHER_LOGGERS = true;  // 是否为新创建的 logger 添加默认 sink

    // 尝试获取已存在的 logger
    auto logger = spdlog::get(logger_name);
    if (logger == nullptr) {
        // 如果 logger 不存在，则创建一个 "null_sink_mt" 日志（即不会输出任何日志）
        logger = spdlog::create_async<spdlog::sinks::null_sink_mt>(logger_name);
        logger->set_level(spdlog::level::trace);  // 设置日志级别为 trace（最详细）
        logger->flush_on(spdlog::level::info);    // 在 info 级别时自动刷新日志缓冲
    }

    // 如果不启用默认 sink，则直接返回 logger
    if constexpr (!ENABLE_DEFAULT_SINK_FOR_OTHER_LOGGERS) {
        return logger;
    }

    // 将默认 sinks 添加到 logger 中（避免重复添加）
    auto& logger_sinks = logger->sinks();
    for (auto sink : default_sinks) {
        if (std::find(logger_sinks.begin(), logger_sinks.end(), sink) == logger_sinks.end()) {
            logger_sinks.push_back(sink);
        }
    }

    return logger;
}

/**
 * @brief 初始化日志系统。
 * 
 * @param log_config_path 日志配置文件路径。
 * 
 * 该方法用于初始化 `spdlog` 日志系统：
 * - **如果提供了日志配置文件**（路径不为 `"empty"`），则从文件中加载日志配置。
 * - **否则，使用默认的日志组件**（如控制台输出、文件输出）。
 */
void LoggerFactory::init(const std::string& log_config_path) {
    if (log_config_path != "empty") {
        spdlog_setup::from_file(log_config_path);  // 从指定的配置文件初始化日志
    }
    init_default_components();  // 初始化默认的日志组件
}

/**
 * @brief 关闭日志系统，释放资源。
 * 
 * 该方法执行以下操作：
 * - 清空 `default_sinks` 集合，移除所有默认的日志输出设备。
 * - 调用 `spdlog::drop_all()`，清除所有已注册的 logger。
 * - 调用 `spdlog::shutdown()`，完全关闭日志系统，释放资源。
 */
void LoggerFactory::shutdown(void) {
    default_sinks.clear();  // 清空默认的日志输出设备
    spdlog::drop_all();     // 删除所有注册的日志器
    spdlog::shutdown();     // 彻底关闭 spdlog
}

/**
 * @brief 初始化默认日志组件。
 * 
 * 该方法创建以下默认日志输出设备（sinks）：
 * 1. **控制台日志**（带颜色的终端输出）
 * 2. **滚动日志文件**（`log/log.log`，大小限制 10MB，最多 10 个文件）
 * 3. **错误日志文件**（`log/err.log`，专门用于存储 error 级别日志）
 * 
 * 此外，该方法还：
 * - **初始化 spdlog 线程池**（用于异步日志）
 * - **设置日志格式**（日志时间戳、级别、logger 名称等）
 */
void LoggerFactory::init_default_components() {
    // 创建带颜色的控制台日志输出 sink
    auto sink_color_console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink_color_console->set_level(spdlog::level::info);  // 设置默认日志级别为 info
    default_sinks.insert(sink_color_console);  // 添加到默认 sink 集合

    // 创建滚动文件日志 sink（日志文件大小 10MB，最多 10 个备份）
    auto sink_rotate_out = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "log/log.log", 1024 * 1024 * 10, 10);  // "log/log.log"，大小 10MB，最多 10 个日志文件
    sink_rotate_out->set_level(spdlog::level::debug);  // 设置 debug 级别
    default_sinks.insert(sink_rotate_out);  // 添加到默认 sink 集合

    // 创建错误日志文件 sink（存储 error 级别的日志）
    auto sink_rotate_err = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "log/err.log", 1024 * 1024 * 10, 10);  // "log/err.log"，大小 10MB，最多 10 个日志文件
    sink_rotate_err->set_level(spdlog::level::err);  // 设置错误级别
    default_sinks.insert(sink_rotate_err);  // 添加到默认 sink 集合

    // 初始化 spdlog 的线程池，支持异步日志（队列大小 8192，单线程日志处理）
    spdlog::init_thread_pool(8192, 1);

    // 设置日志格式：
    // [%Y-%m-%dT%T%z] —— 日期时间（ISO 8601 格式）
    // [%L] —— 日志级别
    // <%n> —— logger 名称
    // %v —— 日志消息
    spdlog::set_pattern("[%Y-%m-%dT%T%z] [%L] <%n>: %v");
}

}  // namespace AstraSim
