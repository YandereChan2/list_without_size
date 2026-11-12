# `Yc::list_without_size<T, Alloc>::reverse`

```C++
void reverse()noexcept;
```

把整条链表逆序。

实现直接委托给侵入式链表的
[`basic_intrusive_list::reverse`](https://github.com/YandereChan2/intrusive_list/blob/main/doc/intrusive_list_readme/reverse.md)：
对调每个节点的 `prev` / `next`，最后对调哨兵的。

- 只改指针：不分配、不搬移元素；
- 元素地址不变，所以迭代器 / 引用仍然有效，仍指向同一个元素（只是位置反了）；
- 空链 / 单元素无效果；
- `O(n)`，`noexcept`。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <vector>

int main()
{
    Yc::list_without_size<int> l{1, 2, 3, 4};

    auto keep = l.begin();          // 指向 1
    l.reverse();
    assert((std::vector<int>(l.begin(), l.end()) == std::vector<int>{4, 3, 2, 1}));
    assert(*keep == 1);             // 迭代器仍指向同一个元素
    auto after = keep;
    ++after;
    assert(after == l.end());       // 1 现在在末尾

    l.reverse();
    assert(l.front() == 1);
}
```
