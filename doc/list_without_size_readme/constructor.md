# `Yc::list_without_size<T, Alloc>::list_without_size`

```C++
list_without_size() = default;
explicit list_without_size(const Alloc& alloc);

explicit list_without_size(size_type n, const Alloc& alloc = Alloc());
list_without_size(size_type n, const T& value, const Alloc& alloc = Alloc());

template<std::input_iterator InputIt>
list_without_size(InputIt first, InputIt last, const Alloc& alloc = Alloc());

list_without_size(const list_without_size& other);
list_without_size(list_without_size&& other)
    noexcept(std::allocator_traits<Alloc>::is_always_equal::value);

list_without_size(const list_without_size& other, const Alloc& alloc);
list_without_size(list_without_size&& other, const Alloc& alloc);

list_without_size(std::initializer_list<T> il, const Alloc& alloc = Alloc());

template<std::ranges::input_range R>
list_without_size(std::from_range_t, R&& rg, const Alloc& alloc = Alloc());
```

|重载|说明|
|:-|:-|
|默认构造|空链表|
|`(alloc)`|空链表，使用 `alloc`|
|`(n)`|`n` 个默认插入的元素|
|`(n, value)`|`n` 个 `value` 的副本|
|`(first, last)`|复制 `[first, last)` 中的元素|
|拷贝构造|复制元素；分配器取 `select_on_container_copy_construction(other.get_allocator())`|
|移动构造|移动分配器，并接管 `other` 的节点；`other` 变为空|
|`(other, alloc)` 拷贝|用显式分配器复制元素|
|`(other, alloc)` 移动|分配器相等时接管节点，否则逐元素移动|
|`(il)`|复制初始化列表|
|`(from_range, rg)`|从 range 复制（C++23）|

## 移动构造

```C++
list_without_size(list_without_size&& other)noexcept(...);
```

- 分配器相等（或 `is_always_equal`）时直接接管节点：不分配、不搬移元素，O(1)；
- 带分配器的版本在分配器不相等时逐元素移动（被移动的元素从 `other` 中删除）。

## 异常

任何构造函数中途抛异常，已经构造的元素会被销毁、节点归还给分配器，不会泄漏；
源链表（拷贝 / 移动的来源）不受影响。

## 复杂度

- 默认构造、移动构造：常数（逐元素移动的版本为线性）；
- 其余：与元素个数成线性。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <vector>

int main()
{
    Yc::list_without_size<int> a;               // 空
    Yc::list_without_size<int> b(3, 7);         // 7, 7, 7
    Yc::list_without_size<int> c(4);            // 0, 0, 0, 0
    Yc::list_without_size<int> d{1, 2, 3};      // 初始化列表
    Yc::list_without_size<int> e(d);            // 拷贝
    Yc::list_without_size<int> f(std::move(e)); // 移动；e 变空

    std::vector<int> src{4, 5};
    Yc::list_without_size<int> g(src.begin(), src.end());
    Yc::list_without_size<int> h(std::from_range, src);

    assert(a.empty());
    assert(b.front() == 7 && b.back() == 7);
    assert(c.front() == 0);
    assert(f.front() == 1 && f.back() == 3);
    assert(e.empty());
    assert(g.front() == 4 && h.back() == 5);
}
```
