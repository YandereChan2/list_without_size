# `Yc::list_without_size<T, Alloc>::merge` , `sort`

```C++
void merge(list_without_size& other);
template<class Compare>
void merge(list_without_size& other, Compare comp);

void merge(list_without_size&& other);
template<class Compare>
void merge(list_without_size&& other, Compare comp);

void sort();
template<class Compare>
void sort(Compare comp);
```

## `merge`

把**已经有序**的 `other` 稳定合并进 `*this`，合并后 `other` 变为空链表。

- 相等元素优先保留 `*this` 中的（稳定）；
- 只搬节点：不分配、不搬移元素，迭代器保持有效；
- 两个链表的分配器必须相等（与 `std::list` 相同）；
- `&other == *this` 时无效果；
- 复杂度 `O(n + m)`。

## `sort`

对整条链做**稳定**排序：

- 稳定：相等元素保持原有相对顺序；
- 自底向上归并排序，`O(n log n)`，不分配内存；
- 只改链接，迭代器仍然指向同一个元素；
- 比较器接收两个 `const T&`。

## 异常

- `sort` 的比较器抛异常：已拆出去的节点会被全部拼回，元素一个不丢（顺序不保证）；
- `merge` 的比较器抛异常：两条链都保持完整。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <vector>

int main()
{
    // sort：稳定
    Yc::list_without_size<int> l{3, 1, 2};
    l.sort();
    assert((std::vector<int>(l.begin(), l.end()) == std::vector<int>{1, 2, 3}));

    // merge：稳定合并
    Yc::list_without_size<int> a{1, 3, 5};
    Yc::list_without_size<int> b{2, 3, 4};
    a.merge(b);
    assert(b.empty());
    assert((std::vector<int>(a.begin(), a.end()) == std::vector<int>{1, 2, 3, 3, 4, 5}));

    // 自定义比较器
    l.sort([](const int& x, const int& y) { return x > y; });
    assert(l.front() == 3);

    Yc::list_without_size<int> c{0};
    a.merge(std::move(c), [](const int& x, const int& y) { return x < y; });
    assert(a.front() == 0);
}
```
