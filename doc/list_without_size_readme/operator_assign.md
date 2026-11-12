# `Yc::list_without_size<T, Alloc>::operator=`

```C++
list_without_size& operator=(const list_without_size& other);

list_without_size& operator=(list_without_size&& other)noexcept(
    std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value
    || std::allocator_traits<Alloc>::is_always_equal::value);

list_without_size& operator=(std::initializer_list<T> il);
```

## 拷贝赋值

1. 若 `this == &other`，直接返回；
2. 用当前分配器销毁当前的全部元素与节点；
3. 若 `propagate_on_container_copy_assignment` 为真，复制分配器；
4. 逐个复制 `other` 的元素。

中途抛异常时链表保持有效（已复制的元素保留），不会泄漏。

## 移动赋值

1. 若 `this == &other`，直接返回；
2. 用当前分配器销毁当前的全部元素与节点；
3. `propagate_on_container_move_assignment` 为真：接管 `other` 的分配器与节点（O(1)）；
4. 否则若分配器相等（或 `is_always_equal`）：直接接管节点；
5. 否则逐元素移动，`other` 中对应的元素被删除。

`other` 在赋值后变为空链表。

## `initializer_list` 赋值

等价于 `assign(il.begin(), il.end())`。

## 异常

| | |
|:-|:-|
|拷贝赋值|基本保证：抛异常后链表有效，已复制的元素保留，不泄漏|
|移动赋值|`noexcept(propagate_on_container_move_assignment || is_always_equal)`|

## 示例

```C++
#include "list_without_size.h"
#include <cassert>

int main()
{
    Yc::list_without_size<int> a{1, 2, 3};
    Yc::list_without_size<int> b{9};

    a = b;                 // 拷贝赋值
    assert(a.front() == 9);

    a = {4, 5};            // initializer_list
    assert(a.front() == 4 && a.back() == 5);

    a = std::move(b);      // 移动赋值；b 变空
    assert(a.front() == 9);
    assert(b.empty());
}
```
