# collective

## HalvingDoubling

### 概述

HalvingDoubling 是 Astra-Sim 实现的集合通信算法之一，主要用于 All-Reduce 和 Reduce-Scatter 操作。它基于对数据块的逐步二分（Halving）和倍增（Doubling）来高效地完成通信。

### 核心功能

- **支持 All-Reduce、All-Gather 和 Reduce-Scatter**
- **使用对数级数的通信方式减少总传输时间**
- **基于环形拓扑 (RingTopology) 进行数据交换**
- **实现流 (Stream) 的控制和调度**
- **动态计算数据包发送/接收的逻辑**

### 代码解析

- `run(EventType event, CallData* data)`: 处理不同的事件类型，如数据包接收、流初始化等。
- `process_stream_count()`: 处理数据流的状态。
- `process_max_count()`: 计算最大通信轮次，调整通信方向。
- `specify_direction()`: 计算当前通信的方向（顺时针或逆时针）。
- `insert_packet(Callable* sender)`: 发送新的数据包。

### 关键点

- **算法复杂度为 O(log N)，适用于大规模分布式系统**
- **不同的通信模式（All-Reduce, All-Gather, Reduce-Scatter）会影响数据流动**
- **采用基于拓扑的优化策略，以最小化通信延迟**

---

## Algorithm

### 概述

Algorithm 是 Astra-Sim 的集合通信算法基类，为所有具体算法（如 Ring、DoubleBinaryTree、HalvingDoubling 等）提供了统一的接口和基本功能。

### 核心功能

- **提供集合通信算法的抽象基类**
- **定义了基本的 `run()`、`call()` 和 `exit()` 方法**
- **包含基本的流初始化逻辑**

### 代码解析

- `init(BaseStream* stream)`: 绑定数据流对象，提供基础的调度支持。
- `call(EventType event, CallData* data)`: 处理事件（具体实现由子类提供）。
- `exit()`: 结束当前通信流程，通知下一阶段继续执行。

### 关键点

- **为所有集合通信算法提供通用接口**
- **支持 Astra-Sim 的异步通信机制**
- **提供基本的事件回调机制**

---

## AllToAll

### 概述

AllToAll 是 Astra-Sim 中的集合通信算法，专用于 `All-to-All` 操作，允许每个节点向所有其他节点发送数据。

### 核心功能

- **支持所有节点之间的数据交换**
- **继承自 Ring 算法，扩展以支持 `All-to-All`**
- **支持环形拓扑 (RingTopology)**
- **实现流式调度 (Stream Scheduling) 逻辑**

### 代码解析

- `run(EventType event, CallData* data)`: 处理 All-To-All 事件，如数据包接收、流初始化等。
- `process_max_count()`: 处理最大流调度逻辑，确保数据完整交换。
- `get_non_zero_latency_packets()`: 计算非零延迟数据包的数量。

### 关键点

- **适用于 GPU、NPU 集群的大规模分布式通信**
- **基于环形拓扑优化数据流动**
- **支持流并行优化以提升带宽利用率**

---

## ChakraImpl

### 概述

ChakraImpl 是 Astra-Sim 的一个高级集合通信实现，基于 Chakra 执行图 (Execution Trace) 进行调度。它使用 Chakra ET 解析通信任务，并根据依赖关系调度计算和通信。

### 核心功能

- **支持自定义的通信 DAG (Directed Acyclic Graph)**
- **实现事件驱动的通信流调度**
- **利用 `ETFeeder` 解析执行图**
- **支持 `Send`、`Receive`、`Compute` 三类任务**

### 代码解析

- `run(EventType event, CallData* data)`: 读取 ETFeeder，解析并执行任务。
- `issue(std::shared_ptr<Chakra::ETFeederNode> node)`: 解析 ET 任务，并调用相应的 `Send` 或 `Receive` 操作。
- `call(EventType event, CallData* data)`: 处理完成的任务，并发起下一个任务。

### 关键点

- **支持复杂的调度策略**
- **基于 `ETFeeder` 解析执行图，适用于异构计算**
- **支持多层次的任务依赖解析**

---

## DoubleBinaryTreeAllReduce

### 概述

DoubleBinaryTreeAllReduce 是 Astra-Sim 中的一种二叉树结构的 All-Reduce 算法，使用双向二叉树拓扑进行高效的梯度汇总。

### 核心功能

- **使用二叉树结构优化 All-Reduce**
- **支持父子节点之间的高效数据交换**
- **结合 `PacketBundle` 进行数据流管理**
- **支持 `Reduce` 和 `Broadcast` 两个阶段**

### 代码解析

- `run(EventType event, CallData* data)`: 处理 All-Reduce 过程中的不同状态，如数据聚合、广播等。
- `process_max_count()`: 计算当前阶段的最大通信轮数，确保正确同步数据。
- `get_non_zero_latency_packets()`: 计算当前非零延迟的数据包数。

### 关键点

- **适用于大规模 GPU 集群的梯度汇总**
- **二叉树结构减少通信延迟**
- **结合 `PacketBundle` 进行批量数据处理**

---

## Ring

### 概述

Ring 是 Astra-Sim 中的集合通信算法之一，基于 **环形拓扑 (RingTopology)** 进行高效的 **All-Reduce**、**All-to-All** 和 **Reduce-Scatter** 计算通信。其主要思想是依次传递数据，确保每个节点都能完成数据交换。

### 核心功能

- **支持 All-Reduce、All-to-All、All-Gather、Reduce-Scatter**
- **基于环形拓扑的高效通信**
- **通过 `InjectionPolicy` 控制数据注入策略**
- **支持流式调度 (Stream Scheduling)**
- **优化通信方向，减少不必要的数据传输**

### 代码解析

- `run(EventType event, CallData* data)`: 处理不同事件，如数据包接收、流初始化等。
- `get_non_zero_latency_packets()`: 计算非零延迟的数据包数量。
- `process_stream_count()`: 控制数据包流动，确保通信稳定。
- `process_max_count()`: 计算最大数据包传输数量，确保流量均衡。
- `insert_packet(Callable* sender)`: 发送新的数据包，确保环形通信逻辑正常执行。
- `release_packets()`: 释放已经处理完成的数据包，减少阻塞。
- `reduce()`: 处理数据归约操作，并更新数据包计数。
- `iteratable()`: 确保迭代过程中，通信可以持续进行。
- `exit()`: 结束当前通信流程，并通知下一个任务。

### 关键点

- **适用于 GPU、NPU 集群的大规模分布式通信**
- **环形拓扑减少通信延迟，提高数据吞吐量**
- **支持不同的注入策略 (`Aggressive`, `Normal`)，优化数据流动**
- **支持流并行优化，提升带宽利用率**