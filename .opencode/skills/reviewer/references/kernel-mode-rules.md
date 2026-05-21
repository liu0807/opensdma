# 内核态检视规则

## 激活条件

当检视的代码变更中包含以下任何一种模式时，**应当激活内核态检视**：

| 触发模式 | 示例 |
|---------|------|
| 用户/内核内存拷贝 | `copy_from_user` `copy_to_user` `__copy_to_user` |
| 内核内存分配 | `kmalloc` `kzalloc` `kfree` `vmalloc` `__get_free_pages` |
| 内核同步原语 | `spin_lock` `spin_unlock` `mutex_lock` `rwlock_t` `rcu_read_lock` |
| 内核文件操作 | `struct file_operations` `.open` `.release` `.ioctl` `.mmap` |
| 内核设备模型 | `miscdevice` `platform_driver` `device_create` `class_create` |
| 内核模块架构 | `module_init` `module_exit` `MODULE_LICENSE` `EXPORT_SYMBOL` |
| __user 注解 | `__user` 指针类型标记 |
| 内核日志 | `pr_*` `dev_*` `printk(KERN_*)` |
| 内核错误处理 | `PTR_ERR` `IS_ERR` `ERR_PTR` |
| 进程/文件描述符 | `struct inode` `struct file` `fdget` |
| 等待队列 | `wait_queue_head_t` `wake_up` `wait_event` |
| 中断上下文 | `request_irq` `IRQ_NONE` `irqreturn_t` `tasklet` `workqueue` |
| 页表/内存描述符 | `struct mm_struct` `get_user_pages` `put_page` |

**说明**: 当前 SDMA 项目为纯用户态代码，不包含内核代码。此规则为**前瞻性预备**。如后续项目扩充添加内核驱动代码（如 SDMA 内核模块、VFIO 插件），应据此规则进行检视。

---

## 目录

- [K1: 用户/内核内存传输](#k1-用户内核内存传输)
- [K2: 内核内存分配](#k2-内核内存分配)
- [K3: 内核锁与同步](#k3-内核锁与同步)
- [K4: 设备驱动模型](#k4-设备驱动模型)
- [K5: 内核错误处理](#k5-内核错误处理)
- [K6: 模块与导出](#k6-模块与导出)
- [K7: 中断与延迟工作](#k7-中断与延迟工作)
- [K8: 文件操作 IOCTL](#k8-文件操作-ioctl)

---

## K1: 用户/内核内存传输

**触发模式**: `copy_from_user` `copy_to_user` `__copy_*` `__user` `access_ok`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K1.1 | copy_from_user/to_user 返回值是否检查（未拷贝的剩余字节） | CRITICAL | 返回非0表示未拷贝完，数据不完整 |
| K1.2 | __user 指针是否在 deref 前经过验证 | CRITICAL | 直接解引用用户指针=内核崩溃 |
| K1.3 | 用户空间的 size 是否超过内核缓冲区 | CRITICAL | 用户传入过大 size 导致内核缓冲区溢出 |
| K1.4 | access_ok 是否在 copy 前调用 | HIGH | 检查用户地址段合法性 |
| K1.5 | 用户指针是否在 copy 后又被使用（TOCTOU） | HIGH | 两次 copy 之间用户空间可能被修改 |
| K1.6 | copy_from_user 的目标是否在栈上导致栈溢出 | CRITICAL | 大 size 传入导致内核栈溢出 |
| K1.7 | 用户提供的指针/大小是否涉及整数溢出 | CRITICAL | size 计算溢出后分配过小缓冲区 |

---

## K2: 内核内存分配

**触发模式**: `kmalloc` `kzalloc` `kcalloc` `kfree` `vmalloc` `vfree` `__get_free_pages` `free_pages` `kstrdup` `kfree_const`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K2.1 | kmalloc/kzalloc 返回值是否检查 NULL | CRITICAL | 内核内存不足 |
| K2.2 | GFP 标志是否正确（GFP_KERNEL vs GFP_ATOMIC） | HIGH | 中断上下文用 GFP_KERNEL 会休眠 |
| K2.3 | kfree 是否在所有路径上调用 | CRITICAL | 内核内存泄漏导致系统逐渐崩溃 |
| K2.4 | vmalloc 是否配对 vfree | HIGH | 虚拟地址空间泄漏 |
| K2.5 | kcalloc 乘法溢出防护 | MEDIUM | kcalloc 内部有溢出检查，kmalloc 无 |
| K2.6 | 分配大小是否来自用户态且经过验证 | CRITICAL | 用户控制的内核分配大小 |
| K2.7 | 内核栈上是否分配了过大的局部数组 | HIGH | 内核栈小（4KB/8KB/16KB），大数组导致栈溢出 |
| K2.8 | devm_* 自动释放 API 使用 | MEDIUM | devm_kzalloc 减少手动释放 |
| K2.9 | page 级: __get_free_pages/free_pages pair | HIGH | 页面级泄漏 |

---

## K3: 内核锁与同步

**触发模式**: `spin_lock` `spin_unlock` `spin_lock_irqsave` `spin_unlock_irqrestore` `mutex_lock` `mutex_unlock` `rwlock_t` `read_lock` `write_lock` `rcu_read_lock` `rcu_read_unlock`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K3.1 | spin_lock 是否在中断上下文中使用，且禁用中断 | CRITICAL | spin_lock (非 irqsave) 在中断中导致死锁 |
| K3.2 | mutex_lock 是否在中断上下文中使用 | CRITICAL | mutex 可睡眠，不能在中断中使用 |
| K3.3 | 持锁期间是否调用可能睡眠的函数 | CRITICAL | spin_lock 持锁时调用 kmalloc(GFP_KERNEL) |
| K3.4 | 锁获取顺序是否一致（锁排序） | HIGH | 不同代码路径获取多个锁的顺序不同=死锁 |
| K3.5 | unlock 是否在**所有**退出路径上调用 | CRITICAL | 提前 return 忘记 unlock |
| K3.6 | RCU 读侧临界区是否调用了可能睡眠的函数 | HIGH | rcu_read_lock 中不能调度 |
| K3.7 | spin_lock_irqsave 的 flags 是否在 unlock 时正确传递 | HIGH | flags 必须是同一变量 |
| K3.8 | 自旋锁保护的共享数据是否为 volatile 或有正确内存屏障 | MEDIUM | 编译器优化导致不一致 |
| K3.9 | 嵌套锁是否造成反向锁顺序 | HIGH | A→B 和 B→A 两个加锁路径 |
| K3.10 | 原子操作 (atomic_t/atomic64_t) 是否替代需要 | MEDIUM | 没必要用锁时可用原子操作 |

---

## K4: 设备驱动模型

**触发模式**: `miscdevice` `platform_driver` `platform_device` `device_create` `class_create` `cdev_init` `cdev_add` `module_platform_driver` `of_match_table`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K4.1 | file_operations 结构体是否使用正确的初始化语法 | HIGH | .owner = THIS_MODULE 必须 |
| K4.2 | open/release 是否配对计数或资源管理 | HIGH | 每次 open 分配的资源在 release 释放 |
| K4.3 | miscdevice/character device 是否在 module_exit 中正确注销 | CRITICAL | 注销遗漏导致设备节点残留 |
| K4.4 | probe 失败时是否清理已分配的资源 | CRITICAL | probe 错误路径必须全部清理 |
| K4.5 | probe/remove 是否成对（devm API 可自动处理） | HIGH | remove 中释放 probe 分配的资源 |
| K4.6 | 多设备实例间的数据隔离 | HIGH | 全局变量导致设备实例互相干扰 |
| K4.7 | of_match_table/id_table 是否与设备树兼容 | MEDIUM | 兼容性字符串匹配 |
| K4.8 | 电源管理回调 (suspend/resume) 是否正确保存/恢复状态 | MEDIUM | 状态一致性 |

---

## K5: 内核错误处理

**触发模式**: `PTR_ERR` `IS_ERR` `ERR_PTR` `IS_ERR_OR_NULL`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K5.1 | 返回指针的函数是否正确使用 IS_ERR/PTR_ERR 而非 NULL 检查 | HIGH | ERR_PTR 编码的错误用 NULL 检查会漏掉 |
| K5.2 | IS_ERR 和 NULL 检查是否混用（IS_ERR_OR_NULL 或分别检查） | MEDIUM | 取决于函数可能返回 NULL 或 ERR_PTR |
| K5.3 | 错误码是否使用标准 errno（-EINVAL/-ENOMEM/-EFAULT） | MEDIUM | 自定义负数错误码导致用户态困惑 |
| K5.4 | kernel log 是否使用正确级别（pr_err vs pr_warn vs pr_info） | LOW | 错误级别混乱 |
| K5.5 | BUG_ON/WARN_ON 使用是否合理 | MEDIUM | BUG_ON 在内核中应极少使用 |
| K5.6 | 错误传播: 被调用函数返回错误，调用方是否传递或处理 | HIGH | 静默忽略错误 |
| K5.7 | 资源获取(如 clk_get/regulator_get)的返回值是否检查 | HIGH | NULL/ERR_PTR 未检查 |

---

## K6: 模块与导出

**触发模式**: `module_init` `module_exit` `module_author` `MODULE_DESCRIPTION` `MODULE_LICENSE` `MODULE_VERSION` `EXPORT_SYMBOL` `EXPORT_SYMBOL_GPL`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K6.1 | module_init/module_exit 是否配对定义 | HIGH | 缺少 exit 导致模块无法卸载 |
| K6.2 | MODULE_LICENSE 是否与代码实际许可证一致 | MEDIUM | GPL 导出(EXPORT_SYMBOL_GPL)需要 GPL 许可 |
| K6.3 | EXPORT_SYMBOL 是否在 .c 文件中（而非 .h） | MEDIUM | 惯例要求 |
| K6.4 | 导出的符号是否有恰当的头文件声明 | MEDIUM | 模块使用者需要可见的声明 |
| K6.5 | MODULE_DESCRIPTION 是否准确描述模块功能 | LOW | 模块信息完整 |
| K6.6 | 模块参数 (module_param) 是否验证用户输入的范围 | HIGH | 模块参数来自用户态 |
| K6.7 | __init/__exit 函数属性是否正确使用 | LOW | __init 标记只在初始化阶段使用的代码 |
| K6.8 | 模块引用计数 (try_module_get/module_put) 是否正确 | HIGH | 模块卸载时仍有引用 |

---

## K7: 中断与延迟工作

**触发模式**: `request_irq` `free_irq` `irqreturn_t` `IRQ_HANDLED` `IRQ_NONE` `tasklet` `tasklet_init` `tasklet_schedule` `workqueue` `INIT_WORK` `schedule_work` `flush_work`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K7.1 | request_irq 返回值是否检查 | CRITICAL | 中断注册失败导致硬件无法工作 |
| K7.2 | free_irq 是否在 module_exit/remove 中调用 | CRITICAL | 模块卸载后中断来袭=系统崩溃 |
| K7.3 | 中断处理程序中是否调用了睡眠函数 | CRITICAL | 中断上下文不能睡眠 |
| K7.4 | 共享中断的 dev_id 是否唯一 | HIGH | free_irq 需要正确 dev_id |
| K7.5 | 中断下半部 (tasklet) 中是否使用了 spin_lock（而非 irqsave） | HIGH | tasklet 中 spin_lock 安全但可能导致优先级反转 |
| K7.6 | workqueue 是否被正确 flush/cancel 在模块退出前 | HIGH | work 在模块卸载后执行=崩溃 |
| K7.7 | 中断处理程序返回 IRQ_HANDLED/IRQ_NONE 是否正确 | MEDIUM | 错误返回值导致内核中断系统异常 |
| K7.8 | threaded IRQ (request_threaded_irq) 的 handler/thread_fn 职责划分 | MEDIUM | 快速处理 vs 可睡眠处理 |

---

## K8: 文件操作与 IOCTL

**触发模式**: `struct file_operations` `.unlocked_ioctl` `.compat_ioctl` `.mmap` `.open` `.release` `.read` `.write`

### 检查清单

| # | 检查项 | 严重等级 | 说明 |
|---|--------|---------|------|
| K8.1 | unlocked_ioctl 中是否验证 cmd 和 arg | CRITICAL | 非法 cmd 导致越界访问 |
| K8.2 | 用户传入的 arg 指针是否经过 copy_from_user | CRITICAL | 直接解引用用户指针 |
| K8.3 | ioctl cmd 使用 `_IOR/_IOW/_IOWR` 宏定义且包含类型检查 | HIGH | 传统数字 cmd 缺少类型校验 |
| K8.4 | compat_ioctl 是否为 32/64 位兼容实现 | MEDIUM | 32位应用在64位内核上调用 |
| K8.5 | mmap 的 offset/len 是否验证范围和对齐 | CRITICAL | mmap 越界映射物理内存 |
| K8.6 | read/write 操作是否遵循非阻塞语义 | MEDIUM | O_NONBLOCK 时返回 -EAGAIN |
| K8.7 | 文件私有数据 (filp->private_data) 初始化/释放 | HIGH | open 中设置，release 中清理 |
| K8.8 | 多线程并发访问 file_operations 是否需要锁 | HIGH | 共享数据结构需要保护 |

---

## 严重等级指南

| 等级 | 内核态定义 | 例子 |
|------|-----------|------|
| **CRITICAL** | 系统崩溃/内核内存破坏/提权漏洞 | copy_from_user 未检查返回值、spin_lock 在中断中死锁、kmalloc NULL 解引用 |
| **HIGH** | 资源泄漏/功能异常/特定条件下崩溃 | probe 错误路径未清理、锁顺序可能死锁、IS_ERR 未检查 |
| **MEDIUM** | 编码规范/可维护性/非关键路径问题 | EXPORT_SYMBOL 位置错误、log 级别不当 |
| **LOW** | 风格/注释问题 | __init 属性遗漏、注释不足 |

### 激活策略

内核态规则**默认不激活**，仅在代码变更中检测到 K1-K8 触发模式时激活。激活后在报告中注明：

```
## 激活的内核态规则
- [K1: 用户/内核内存传输] - 检测到 copy_from_user
- [K3: 内核锁与同步] - 检测到 spin_lock
```

如果未检测到任何内核模式，跳过所有内核态规则，不在最终报告中出现。
