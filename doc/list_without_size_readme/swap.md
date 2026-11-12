# `Yc::list_without_size<T, Alloc>::swap`

```C++
void swap(list_without_size& other)noexcept(
    std::allocator_traits<Alloc>::propagate_on_container_swap::value
    || std::allocator_traits<Alloc>::is_always_equal::value);

template<class T, class Alloc>
void swap(list_without_size<T, Alloc>& lhs, list_without_size<T, Alloc>& rhs)
    noexcept(noexcept(lhs.swap(rhs)));
```

交换两个链表的内容。

- 只交换哨兵 / 节点链接与（必要时）分配器：`O(1)`，不分配、不搬移元素；
- 迭代器 / 引用保持有效，并且**跟随元素**而不是容器：`a.swap(b)` 之后，原来指向 `a` 中元素的
  迭代器现在指向 `b` 中的同一个元素；
- `propagate_on_container_swap` 为真时交换分配器；否则要求两个链表的分配器相等
  （与 `std::list` 相同，不相等时行为未定义，本库在 Debug 下用 `assert` 捕获）；
- 与非成员 `Yc::swap` 配合，可用于泛型代码。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <utility>

int main()
{
    Yc::list_without_size<int> a{1, 2, 3};
    Yc::list_without_size<int> b{4, 5};

    auto keep = a.begin();       // 指向 1
    a.swap(b);
    assert(a.front() == 4);
    assert(b.front() == 1);
    assert(*keep == 1);          // 迭代器跟着元素走

    swap(a, b);                  // ADL 找到 Yc::swap
    assert(a.front() == 1);

    a.swap(a);                   // 自交换无效果
    assert(a.front() == 1);
}
```
