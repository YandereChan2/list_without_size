# `Yc::list_without_size<T, Alloc>::empty` , `max_size`（以及没有 `size`）

```C++
bool empty()const noexcept;
size_type max_size()const noexcept;
```

- `empty()`：是否为空。判断哨兵是否脱管，O(1)。
- `max_size()`：最大可能元素数，来自节点分配器的 `max_size`。
- **没有 `size()`**：容器不记录元素个数（见[设计说明](easy_for_understand.md#为什么没有-size)）。

需要个数时用 `std::distance`，代价 O(n)：

```C++
auto n = std::distance(l.begin(), l.end());
```

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <iterator>

int main()
{
    Yc::list_without_size<int> l;
    assert(l.empty());
    assert(l.max_size() > 0);

    l.push_back(1);
    assert(!l.empty());
    assert(std::distance(l.begin(), l.end()) == 1);

    l.clear();
    assert(l.empty());
}
```
