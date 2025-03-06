### Main File ###
1. The main file of ns3 is **AstraSimNetwork.cc**. This file containt the ASTRA-SIM network API implementation. It also defines worklaods and system input files.
2. The network input file is fed in `build/astra_ns3/build.sh` file with the envvar **NETWORK**. The current input is 'mix/config.txt' (https://github.com/astra-sim/astra-network-ns3/blob/main/simulation/mix/config.txt). Inside the 'mix/config.txt', there is a parameter called **TOPOLOGY_FILE** which takes the topology file that describes the physical topology (link connection, switch, nodes, etc). The actual file is located within the ns3 submodule due to licensing issues. 
3. The logical topology file, fed in `build.sh` under the envvar **LOGICAL_TOPOLOGY**, describes how many of these NPUs we are actually going to use, in what logical topology. This file is the source for the vector `physical_dims``. If this value is {64}, this allocates the first 64 NPUs to the workload. If this value is {8,8}, it will allocate the same number of NPUs, but the topology will be 2D. (Think of a scenario where we have a physical cluster of 128 nodes, but use only 64 of them. The physical topology defined in **TOPOLOGY_FILE** will have 128 nodes, but the logical topology defined in **LOGICAL_TOPOLOGY** will only indicate 64 nodes.)
4. The system input file should match the dimension as defined in `physical_dims`. Currently there is no checker to ensure correctness.

### **1. `AstraSimNetwork.cc` 是 NS3 的主文件**

- 这个文件实现了 **ASTRA-SIM** 的 **网络 API**，也就是说，它是 ASTRA-SIM 和 NS3 交互的核心代码。
- 它定义了 **工作负载（workloads）** 和 **系统输入文件（system input files）**，这些文件用于模拟分布式计算任务。

---

### **2. 网络输入文件由 `build/astra_ns3/build.sh` 通过 `NETWORK` 变量提供**

- `NETWORK` 是一个环境变量，它指定了 **NS3 的网络配置文件**。
- 当前默认的网络配置文件是：
    
    ```
    arduino
    复制编辑
    mix/config.txt
    
    ```
    
    你可以在 GitHub 上找到它：
    [config.txt](https://github.com/astra-sim/astra-network-ns3/blob/main/simulation/mix/config.txt)
    
- **这个文件的作用：**
    - 它指定了仿真网络的拓扑结构和参数。
    - 其中有一个关键参数 **`TOPOLOGY_FILE`**，它用于指定**物理拓扑**文件。
- **`TOPOLOGY_FILE` 物理拓扑文件**
    - 这个文件描述了 **物理网络拓扑**（比如计算节点如何连接，交换机和链路的配置等）。
    - 由于许可证（Licensing）问题，这个文件存储在 **NS3 子模块**中，而不在 ASTRA-SIM 的主代码库中。

---

### **3. 逻辑拓扑文件 (`LOGICAL_TOPOLOGY`)**

- 逻辑拓扑文件也是由 `build.sh` 提供，并通过环境变量 `LOGICAL_TOPOLOGY` 传递。
- 这个文件决定了**有多少 NPU（计算单元）会被用于仿真**，以及它们的逻辑拓扑结构。
- **逻辑拓扑 vs 物理拓扑**
    - 物理拓扑（在 `TOPOLOGY_FILE` 里定义）是整个集群的真实网络结构。例如，它可能包含 128 个计算节点。
    - 逻辑拓扑（在 `LOGICAL_TOPOLOGY` 里定义）是仿真中实际使用的 NPU 数量和拓扑。例如：
        - `physical_dims = {64}` → 只使用前 64 个 NPU，按 1D 拓扑结构排列。
        - `physical_dims = {8,8}` → 使用 64 个 NPU，但以 2D 格式（8×8）排列。
    - 这种设计允许**在更大的物理集群中模拟较小规模的训练任务**。
- **示例**
    - **假设我们有 128 个物理节点，但只用 64 个**
        - `TOPOLOGY_FILE` 定义了 128 个节点的物理网络（全部可用）。
        - `LOGICAL_TOPOLOGY` 只分配 64 个节点给计算任务，可能是 `physical_dims = {8,8}`（表示 8×8 的 2D 逻辑拓扑）。

---

### **4. 系统输入文件（System Input File）**

- **必须与 `physical_dims` 定义的维度匹配**。
- **目前没有检查机制**来确保拓扑结构和系统输入文件的匹配正确性，这可能导致错误（比如，物理拓扑定义了 128 个节点，但系统输入文件只定义了 64 个，导致编号不匹配）。
- 这意味着**用户需要手动确保 `LOGICAL_TOPOLOGY` 和 `TOPOLOGY_FILE` 之间的一致性**，否则可能会出错。