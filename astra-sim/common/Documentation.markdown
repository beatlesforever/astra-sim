### 1. **AstraNetworkAPI.hh**

- 该类提供了网络通信的抽象接口，允许不同的后端（如 Garnet、NS3、Analytical）实现具体的通信逻辑。
- **核心功能**：
    - `sim_send()` 和 `sim_recv()`：用于数据发送和接收，支持异步回调 `msg_handler`。
    - `sim_schedule()`：用于在网络后端安排事件。
    - `sim_get_time()`：获取当前网络模拟的时间。
    - `get_BW_at_dimension(int dim)`：获取特定维度的带宽（默认返回 -1，需要子类实现）。
    - `sim_notify_finished()`：用于通知模拟结束。
- **关键点**：
    - 这是一个 **抽象基类**，所有实际网络实现（如 NS3 或 Analytical）都需要继承它并实现其虚函数。

---

### 2. **AstraRemoteMemoryAPI.hh**

- 该类提供了远程内存管理的抽象接口，允许 AstraSim 进行跨节点的张量（tensor）管理。
- **核心功能**：
    - `set_sys(int id, Sys* sys)`：设置系统实例。
    - `issue(uint64_t tensor_size, WorkloadLayerHandlerData* wlhd)`：向远程内存发起访问请求。
- **关键点**：
    - 该类是 **纯虚基类**，必须由子类实现具体的远程内存操作。
    - `WorkloadLayerHandlerData` 可能涉及计算任务的层次管理。

---

### 3. **AstraSimDataAPI.hh**

- 该类用于存储 AstraSim 模拟过程中收集的数据，主要涉及计算、通信和延迟信息。
- **核心数据**：
    - `LayerData` 结构体：记录各层计算和通信的时间数据，如：
        - `total_forward_pass_compute`（前向传播计算时间）
        - `total_fwd_comm`（前向传播通信时间）
        - `avg_queuing_delay`（排队延迟）
        - `avg_network_message_dealy`（网络消息延迟）
    - `AstraSimDataAPI`：
        - `run_name`（模拟运行名称）
        - `layers_stats`（所有层的统计数据）
        - `workload_finished_time`（模拟完成时间）
        - `total_compute` 和 `total_exposed_comm`（总计算时间和暴露的通信时间）
- **关键点**：
    - 这个类主要用于**存储模拟数据**，并不涉及具体计算。

---

### 4. **Common.hh**

- **核心功能**：
    - 定义了时间单位（`time_type_e`）、请求类型（`req_type_e`）等基础数据结构。
    - `timespec_t` 结构体用于存储时间信息。
    - `sim_request` 结构体用于存储模拟请求的信息，包括源/目标 rank、tag、请求类型等。
    - **集合通信实现相关的定义**：
        - `ComType`：支持 `Reduce_Scatter`、`All_Gather`、`All_Reduce` 等。
        - `CollectiveImplType`：包括 `Ring`、`OneRing`、`DoubleBinaryTree` 等常见的集合通信算法。
        - `CollectiveOptimization`：是否进行 `LocalBWAware` 优化。
        - `SchedulingPolicy`：支持 `FIFO`、`LIFO`、`EXPLICIT` 调度策略。
    - **集体通信实现类**：
        - `CollectiveImpl`：提供基本的集合通信实现接口。
        - `DirectCollectiveImpl`：增加了 `direct_collective_window` 参数，用于直接集合通信。
        - `ChakraCollectiveImpl`：允许用户提供外部定义的集合通信实现文件。
- **关键点**：
    - 这个文件包含大量 **基础数据结构**，并定义了**集合通信的多种实现方式**，是 AstraSim **通信层的核心**。

---

### 5. **Logging.cc & Logging.hh**

- 这两个文件负责日志管理，采用 `spdlog` 进行日志记录。
- **核心功能**：
    - `LoggerFactory::get_logger()`：获取指定名称的 logger。
    - `LoggerFactory::init()`：初始化日志系统，可以从配置文件加载日志级别。
    - `LoggerFactory::shutdown()`：关闭日志系统，清理资源。
    - `LoggerFactory::init_default_components()`：
        - 终端彩色日志（`stdout_color_sink_mt`）
        - 滚动日志文件（`log/log.log` 和 `log/err.log`）
- **关键点**：
    - 该日志系统支持 **异步日志**，可以大规模记录模拟过程中的事件信息。