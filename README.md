# Yc::list_without_size

接口与 `std::list` 一致的拥有型双向链表，**不提供 `size()`**：链表不记录元素个数，插入 / 删除 / `splice` 只改指针。

# 目标

- 和 C++ 标准库保持风格一致（对齐 C++23 的 `std::list` 接口）
- 双向迭代器、元素地址稳定、`splice` 为 O(1)
- 完整的分配器语义：传播、有状态分配器、缀饰指针
- header-only

# 非目标

- 不提供 `size()`（需要个数时用 `std::distance`）
- 不提供非成员的比较运算符、`std::erase` / `std::erase_if`
- 不是侵入式容器：元素由本容器分配与析构

# 依赖

本库建立在侵入式链表之上，依赖：

- [`YandereChan2/intrusive_list`](https://github.com/YandereChan2/intrusive_list)
  - 头文件：[`intrusive_list.h`](https://github.com/YandereChan2/intrusive_list/blob/main/intrusive_list.h)
  - 文档：[`intrusive_list_readme/intrusive_list.md`](https://github.com/YandereChan2/intrusive_list/blob/main/doc/intrusive_list_readme/intrusive_list.md)

本仓库用 git submodule 引用它（`intrusive_list/`）。克隆后需要初始化子模块，并把该目录加入
头文件搜索路径。

# 文件

| 文件 | 说明 |
|---|---|
| [`list_without_size.h`](list_without_size.h) | 容器实现 |
| [`test_list_without_size.cpp`](test_list_without_size.cpp) | 自测：与 `std::list` 的随机差分、异常安全、分配器、缀饰指针 |
| [`doc/list_without_size_readme/`](doc/list_without_size_readme/list_without_size.md) | 使用文档 |
| `intrusive_list/` | 依赖（git submodule） |

# 快速开始

```C++
#include "list_without_size.h"
#include <cassert>
#include <iterator>

int main()
{
    Yc::list_without_size<int> l{3, 1, 2};

    l.push_back(4);
    l.push_front(0);
    assert(l.front() == 0);
    assert(l.back() == 4);
    assert(std::distance(l.begin(), l.end()) == 5);   // 没有 size()

    l.sort();
    assert(l.front() == 0 && l.back() == 4);

    l.reverse();
    assert(l.front() == 4);

    l.remove_if([](int v) { return v % 2 == 0; });
    assert(std::distance(l.begin(), l.end()) == 2);

    l.clear();
    assert(l.empty());
}
```

# 构建测试

先取回依赖：

```bat
git submodule update --init
```

使用 MSVC（C++23）：

```bat
cl /nologo /std:c++latest /EHsc /W4 /utf-8 /I . /I intrusive_list test_list_without_size.cpp
test_list_without_size.exe
```

使用 CMake：

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

预期输出：

```text
test_list_without_size: ok
```

# 文档

- [完整文档](doc/list_without_size_readme/list_without_size.md)
- [设计说明](doc/list_without_size_readme/easy_for_understand.md)（为什么没有 `size()`、适配方式、异常安全）

# 许可证

MIT，见 [LICENSE](LICENSE)。
