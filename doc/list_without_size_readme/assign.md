# `Yc::list_without_size<T, Alloc>::assign` , `assign_range`

```C++
void assign(size_type n, const T& value);

template<std::input_iterator InputIt>
void assign(InputIt first, InputIt last);

void assign(std::initializer_list<T> il);

template<std::ranges::input_range R>
void assign_range(R&& rg);
```

用新内容替换链表中的全部元素。

## 实现方式

不是"先清空再插入"，而是：

1. 逐个把已有元素赋值为新值；
2. 元素不够时在尾部补插；
3. 元素多了就删掉尾部多出来的部分。

这样即使实参引用了链内元素也是安全的：

```C++
Yc::list_without_size<int> l{1, 2, 3};
l.assign(4, l.front());    // 实参是链内元素，不会悬空
```

## 复杂度

`O(n + m)`，`n` 为原元素数，`m` 为新元素数。

## 异常

基本保证：抛异常时链表仍然有效，已完成的赋值 / 插入保留，不泄漏。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <ranges>
#include <vector>

int main()
{
    Yc::list_without_size<int> l{1, 2, 3};

    l.assign(2, 7);                         // 7, 7
    assert(l.front() == 7 && l.back() == 7);

    std::vector<int> v{4, 5, 6};
    l.assign(v.begin(), v.end());           // 4, 5, 6
    assert(l.front() == 4 && l.back() == 6);

    l.assign({8, 9});                       // 8, 9
    assert(l.front() == 8);

    l.assign_range(std::views::iota(0, 3)); // 0, 1, 2
    assert(l.front() == 0 && l.back() == 2);
}
```
