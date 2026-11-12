# `Yc::list_without_size<T, Alloc>::splice`

```C++
void splice(const_iterator pos, list_without_size& other)noexcept;
void splice(const_iterator pos, list_without_size&& other)noexcept;

void splice(const_iterator pos, list_without_size& other, const_iterator it)noexcept;
void splice(const_iterator pos, list_without_size&& other, const_iterator it)noexcept;

void splice(const_iterator pos, list_without_size& other, const_iterator first, const_iterator last)noexcept;
void splice(const_iterator pos, list_without_size&& other, const_iterator first, const_iterator last)noexcept;
```

把 `other` 的元素搬到 `pos` 之前：

- 整条链版本：`other` 的全部元素；
- 单点版本：`it` 所指的一个元素；
- 区间版本：`[first, last)`。

语义与 `std::list::splice` 一致：

- 只搬节点，不分配、不复制、不搬移元素；
- 迭代器 / 引用保持有效，搬过去之后仍指向同一个元素；
- 常数时间（区间版本也是常数：双向迭代器 O(1) 就能找到 `last` 的前一个）；
- `other` 在搬移后变为空链表。

## 前置条件

- 两个链表的分配器必须相等（与 `std::list` 相同）；不相等时标准规定为未定义行为，
  本库在 Debug 下用 `assert` 捕获；
- `pos` 是 `*this` 上的有效位置（可以是 `end()`）；
- 单点版本：`pos == it` 或 `pos == ++it` 时无效果；
- 区间版本：`first == last` 或 `pos == first` 时无效果；`pos` 不能落在 `[first, last)` 内；
- 整条链版本：`other` 为空、或 `&other == *this` 时无效果。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <iterator>

int main()
{
    Yc::list_without_size<int> a{1, 2, 3};
    Yc::list_without_size<int> b{4, 5};

    auto keep = a.begin();              // 指向 1
    a.splice(a.end(), b);               // 整条链
    assert(b.empty());
    assert(*keep == 1);                 // 迭代器仍有效
    assert(std::distance(a.begin(), a.end()) == 5);

    // 单点：把最后一个元素搬到表头
    auto last = std::prev(a.end());
    a.splice(a.begin(), a, last);
    assert(a.front() == 5);

    // 区间：[第一个, 第二个)
    a.splice(a.end(), a, a.begin(), std::next(a.begin()));
    assert(a.back() == 5);

    // 右值重载：临时链表
    a.splice(a.end(), Yc::list_without_size<int>{6, 7});
    assert(a.back() == 7);
}
```
