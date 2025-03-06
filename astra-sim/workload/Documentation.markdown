# workload

## **HardwareResource.hh**

### **概述**

`HardwareResource.hh` 定义了 `HardwareResource` 类，该类用于管理计算资源的占用情况，包括 CPU 计算、GPU 计算和 GPU 通信操作。

### **核心功能**

- 记录和管理 NPU（Neural Processing Unit）数量。
- 追踪 CPU 和 GPU 计算/通信任务的进行情况。
- 提供资源占用、释放和可用性查询功能。
- 输出资源统计信息。

### **代码解析**

- `HardwareResource(uint32_t num_npus)`: 初始化计算资源，默认 NPU 数量，并初始化所有计算/通信计数变量。
- `void occupy(const std::shared_ptr<Chakra::ETFeederNode> node)`: 占用某个计算资源，根据 `node` 类型调整 CPU 或 GPU 任务计数。
- `void release(const std::shared_ptr<Chakra::ETFeederNode> node)`: 释放计算资源，并进行断言检查，确保计数正确。
- `bool is_available(const std::shared_ptr<Chakra::ETFeederNode> node) const`: 判断某个计算资源是否可用。
- `void report()`: 输出当前的计算资源统计信息。

### **关键点**

- 该类严格保证同一时刻 CPU 计算任务和 GPU 计算/通信任务的执行规则。
- 使用 `assert` 确保任务的正确性，例如 CPU 计算任务只能有一个进行中。
- 通过 `num_in_flight_*` 变量追踪当前执行中的任务数量，确保资源分配不会冲突。

---

## **HardwareResource.cc**

### **概述**

`HardwareResource.cc` 实现了 `HardwareResource.hh` 头文件中声明的 `HardwareResource` 类的功能。

### **核心功能**

- 维护 CPU 和 GPU 计算任务的占用情况。
- 计算任务的资源调度，包括占用、释放和状态检查。
- 统计计算任务的执行情况。

### **代码解析**

- `HardwareResource::occupy(const shared_ptr<Chakra::ETFeederNode> node)`: 依据 `node` 类型占用计算资源，并进行计数管理。
- `HardwareResource::release(const shared_ptr<Chakra::ETFeederNode> node)`: 释放计算资源，并进行计数调整，确保资源不会错误释放。
- `HardwareResource::is_available(const shared_ptr<Chakra::ETFeederNode> node) const`: 通过 `num_in_flight_*` 变量检查当前资源是否可用。
- `HardwareResource::report()`: 输出当前计算资源的使用统计信息，包括 CPU 任务、GPU 任务和 GPU 通信任务的数量及其执行时间。

### **关键点**

- 确保 GPU 计算和 GPU 通信不能同时进行，以防止资源冲突。
- 对 `COMM_RECV_NODE` 类型的 `node` 进行了特殊处理，使其不会被错误释放。
- 资源的状态变化受 `num_in_flight_*` 变量的严格约束，确保不会发生资源泄露或错误使用。

---

## **Workload.hh**

### **概述**

`Workload.hh` 定义了 `Workload` 类，该类用于模拟和管理深度学习训练的计算任务流，包括计算、内存访问和通信操作。

### **核心功能**

- 解析计算任务文件，并加载到 `ETFeeder`。
- 管理计算任务的调度，包括依赖关系处理。
- 控制计算任务的执行，包括计算、远程内存访问和通信。
- 维护计算任务的状态，并提供调度接口。

### **代码解析**

- `Workload(Sys* sys, std::string et_filename, std::string comm_group_filename)`: 初始化 `Workload`，并加载计算任务文件及通信组信息。
- `~Workload()`: 释放 `Workload` 相关的动态分配资源。
- `void initialize_comm_group(std::string comm_group_filename)`: 解析通信组文件，并初始化 `CommunicatorGroup` 以支持集合通信。
- `void issue_dep_free_nodes()`: 遍历计算任务队列，找到无依赖的任务，并尝试执行。
- `void issue(std::shared_ptr<Chakra::ETFeederNode> node)`: 依据 `node` 类型分发计算或通信任务。
- `void issue_replay(std::shared_ptr<Chakra::ETFeederNode> node)`: 进行计算任务的回放，用于模拟已记录的计算任务执行。
- `void issue_remote_mem(std::shared_ptr<Chakra::ETFeederNode> node)`: 处理远程内存访问任务。
- `void issue_comp(std::shared_ptr<Chakra::ETFeederNode> node)`: 处理计算任务，并依据 `roofline_enabled` 模拟计算性能。
- `void issue_comm(std::shared_ptr<Chakra::ETFeederNode> node)`: 处理集合通信任务，并进行调度管理。
- `void call(EventType event, CallData* data)`: 事件处理函数，完成 `Workload` 任务调度。
- `void fire()`: 触发计算任务的执行。
- `void report()`: 输出 `Workload` 任务执行的详细统计信息。

### **关键点**

- 任务调度基于 `ETFeeder`，按照依赖关系执行任务。
- 支持 `roofline` 计算模型，可以根据操作强度动态计算执行时间。
- 采用 `CommunicatorGroup` 进行集合通信，确保计算任务的高效执行。

---

## **Workload.cc**

### **概述**

`Workload.cc` 实现了 `Workload.hh` 头文件中声明的 `Workload` 类的功能，主要负责计算任务的调度和管理。

### **核心功能**

- 解析并加载计算任务文件。
- 进行任务依赖管理，确保任务按顺序执行。
- 处理 CPU 和 GPU 计算任务。
- 处理远程内存访问任务。
- 处理通信任务，包括点对点和集合通信。

### **代码解析**

- `Workload::Workload(Sys* sys, std::string et_filename, std::string comm_group_filename)`: 加载计算任务和通信组，并初始化 `HardwareResource` 资源管理器。
- `Workload::~Workload()`: 释放 `ETFeeder`、`CommunicatorGroup` 和 `HardwareResource` 资源。
- `void Workload::initialize_comm_group(std::string comm_group_filename)`: 解析 `json` 格式的通信组信息，并创建 `CommunicatorGroup`。
- `void Workload::issue_dep_free_nodes()`: 遍历 `ETFeeder`，找到无依赖的任务并执行。
- `void Workload::issue(std::shared_ptr<Chakra::ETFeederNode> node)`: 调度计算或通信任务。
- `void Workload::issue_comp(std::shared_ptr<Chakra::ETFeederNode> node)`: 依据 `roofline_enabled` 计算计算任务的执行时间。
- `void Workload::issue_comm(std::shared_ptr<Chakra::ETFeederNode> node)`: 处理 `ALL_REDUCE`、`ALL_TO_ALL` 等集合通信任务。
- `void Workload::call(EventType event, CallData* data)`: 事件回调处理，完成计算任务的收尾工作。
- `void Workload::fire()`: 触发任务调度，启动 `Workload` 运行。
- `void Workload::report()`: 输出计算任务执行的统计信息。

### **关键点**

- 计算任务的调度基于 `ETFeeder`，确保任务按依赖关系执行。
- 采用 `roofline` 计算模型优化计算任务的执行时间。
- 支持 `CommunicatorGroup` 进行高效的集合通信调度。