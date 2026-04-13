
```markdown
# MySQL 数据库连接池 (C++)

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![MySQL](https://img.shields.io/badge/MySQL-5.7+-orange.svg)](https://www.mysql.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

一个基于 C++17 实现的高性能 MySQL 数据库连接池，采用线程安全的单例模式、生产者-消费者模型，支持连接动态扩容与空闲超时回收。

---

## ?? 功能特性

- **?? 单例模式**：线程安全的懒汉式单例，全局唯一连接池实例
- **? 连接复用**：基于 `std::shared_ptr` 智能指针管理，自动归还连接
- **?? 动态扩容**：生产者线程在连接不足时动态创建新连接（上限可配置）
- **?? 自动清理**：独立后台线程定期扫描并销毁超时空闲连接
- **?? 超时机制**：获取连接支持超时等待，防止永久阻塞
- **?? 配置化**：通过 `mysql.ini` 文件灵活配置数据库参数与池大小
- **?? 线程安全**：基于 `std::mutex` 与 `std::condition_variable` 实现高效同步

---

## ??? 项目结构

```
MysqlConnectionPool/
├── CommonConnectionPool.h      # 连接池类头文件（核心）
├── CommonConnectionPool.cpp    # 连接池类实现
├── Connection.h                # 数据库连接封装类头文件
├── Connection.cpp              # 数据库连接实现（基于 mysql-connector-c）
├── public.h                    # 日志宏定义工具
├── main.cpp                    # 测试入口（含单线程/多线程对比）
├── mysql.ini                   # 数据库连接配置文件
├── pch.h / pch.cpp             # 预编译头文件（Visual Studio）
└── resource.h                  # Windows 资源文件
```

---

## ?? 配置说明

编辑 `mysql.ini` 文件配置数据库连接参数：

```ini
# 数据库连接信息
ip=localhost
port=3306
database=test
username=root
password=your_password

# 连接池参数
initialSize=10        # 初始连接数（启动时创建）
maxActive=1024        # 最大连接数（上限）
maxIdleTime=60        # 连接最大空闲时间（秒）
connectionTimeout=100 # 获取连接超时时间（毫秒）
```

---

## ?? 快速开始

### 1. 环境依赖

- **C++ 编译器**：支持 C++17 标准（MSVC 2017+ / GCC 7+ / Clang 5+）
- **MySQL 客户端库**：`libmysqlclient` 或 `mysql-connector-c`
- **操作系统**：Windows / Linux / macOS

### 2. 编译构建

**Windows (Visual Studio)：**
```bash
# 创建数据库测试表
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS test; USE test; CREATE TABLE IF NOT EXISTS member (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(50));"
# 打开 .sln 解决方案编译
```

**Linux (g++)：**
```bash
g++ -std=c++17 -pthread *.cpp -o pool_test -lmysqlclient
./pool_test
```

### 3. 使用示例

```cpp
#include "CommonConnectionPool.h"
#include <iostream>

int main() {
    // 获取连接池实例（单例）
    ConnectionPool* pool = ConnectionPool::GetConnectionPool();
    
    // 从池中获取连接（智能指针自动管理生命周期）
    std::shared_ptr<Connection> conn = pool->GetConnection();
    
    if (conn != nullptr) {
        // 执行 SQL 更新
        bool result = conn->Update("INSERT INTO users (name) VALUES ('Alice')");
        
        // 查询操作
        MYSQL_RES* res = conn->Query("SELECT * FROM users");
        // 处理结果集...
        mysql_free_result(res);
        
    } // 连接在此处自动归还到池中（shared_ptr 析构触发删除器）
    
    return 0;
}
```

---

## ?? 性能测试

项目内置了单线程与多线程对比测试：

### 测试配置
- **并发线程**：4 线程
- **总操作数**：10,000 次（每线程 2,500 次）
- **测试环境**：本地 MySQL 8.0，Release 模式

### 运行测试

```cpp
// 测试代码示例（详见 main.cpp）
const int THREAD_COUNT = 4;
const int OPERATIONS_PER_THREAD = 2500;

// 多线程 + 连接池
for (int i = 0; i < THREAD_COUNT; i++) {
    threads.emplace_back(testWithPool, i, OPERATIONS_PER_THREAD);
}
```

### 预期结果

| 模式 | 耗时 | 连接数变化 |
|------|------|------------|
| 不使用连接池 | ~8000-15000 ms | 每操作新建/关闭连接（10000次TCP握手） |
| 使用连接池 | ~500-1200 ms | 初始10个，动态扩增至16-20个并复用 |

**性能提升**：约 **8-15 倍**（取决于网络延迟与数据库性能）

---

## ?? 技术实现细节

### 1. 连接池架构

```
┌─────────────────────────────────────────────┐
│          ConnectionPool (Singleton)         │
├─────────────────────────────────────────────┤
│  ┌─────────────┐      ┌─────────────────┐  │
│  │ Producer    │      │ Cleaner         │  │
│  │ Thread      │      │ Thread          │  │
│  │ (连接生产者) │      │ (空闲清理器)     │  │
│  └──────┬──────┘      └─────────────────┘  │
│         │                                   │
│  ┌──────▼──────────────────────────┐        │
│  │  std::queue<std::unique_ptr<Connection>> │  │
│  │  连接队列（互斥锁保护）            │        │
│  └─────────────────────────────────┘        │
└─────────────────────────────────────────────┘
```

### 2. 核心设计模式

- **生产者-消费者**：当队列为空且当前连接数 < 最大值时，生产者线程异步创建新连接
- **RAII 管理**：`std::shared_ptr` + 自定义删除器，确保连接使用完毕后自动归还队列
- **条件变量**：`_condition.wait()` / `notify_all()` 实现高效线程等待与唤醒

### 3. 线程安全机制

| 组件 | 同步机制 | 说明 |
|------|----------|------|
| 连接队列 | `std::mutex` + `std::unique_lock` | 保护队列的 push/pop 操作 |
| 连接计数 | `std::atomic` (隐含) | 通过锁保护的 `_currentSize` |
| 队列状态 | `std::condition_variable` | 通知生产者/消费者状态变化 |

---

## ?? 注意事项

1. **连接归还时机**：`shared_ptr` 析构时自动归还，建议合理使用作用域控制连接持有时间，避免长时间占用
2. **超时设置**：若 `connectionTimeout` 设置过短，高并发时可能出现获取连接失败返回 `nullptr` 的情况
3. **MySQL 超时**：需确保 MySQL 服务器的 `wait_timeout` 大于连接池的 `maxIdleTime`，防止连接被服务器端提前断开
4. **线程退出**：当前实现中生产者与清理线程为 `detach` 模式，程序退出时可能产生资源泄漏警告（生产环境建议添加优雅退出机制）

---

## ?? 待优化项

- [ ] 添加连接有效性检测（ping），自动剔除失效连接
- [ ] 支持配置热加载（无需重启修改配置）
- [ ] 添加连接池状态监控接口（获取当前连接数、等待队列长度等）
- [ ] 实现优雅关闭（join 后台线程，释放所有连接）
- [ ] 添加异步/回调接口支持

---

## ?? 开源协议

MIT License - 详见 LICENSE 文件

---

## ????? 作者

C++ 数据库连接池学习项目

```

这份 README 涵盖了项目介绍、结构说明、使用方法、性能对比和技术细节，你可以根据实际测试结果填写具体的性能数据。需要我调整哪些部分吗？