# 修改器

## `clear`

```C++
void clear()noexcept;
```

销毁全部元素并释放节点，链表变空。复杂度线性，不抛异常。

## 插入

```C++
template<class... Args>
iterator emplace(const_iterator pos, Args&&... args);

iterator insert(const_iterator pos, const T& value);
iterator insert(const_iterator pos, T&& value);
iterator insert(const_iterator pos, size_type n, const T& value);

template<std::input_iterator InputIt>
iterator insert(const_iterator pos, InputIt first, InputIt last);

iterator insert(const_iterator pos, std::initializer_list<T> il);

template<std::ranges::input_range R>
iterator insert_range(const_iterator pos, R&& rg);
```

在 `pos` 之前插入元素，返回指向插入的第一个元素的迭代器（没有插入时返回 `pos`）。

- `pos` 可以是 `end()`，表示追加到尾部；
- 单个插入先构造节点、成功后才挂链，因此是强异常保证；
- 区间插入中途抛异常时，已插入的元素保留（基本保证）；
- 插入不会使其他迭代器 / 引用失效。

## 追加

```C++
template<std::ranges::input_range R>
void append_range(R&& rg);

template<std::ranges::input_range R>
void prepend_range(R&& rg);
```

把一段元素追加到尾部 / 头部，等价于 `insert(cend(), ...)` / `insert(cbegin(), ...)`。

## 删除

```C++
iterator erase(const_iterator pos)noexcept;
iterator erase(const_iterator first, const_iterator last)noexcept;
```

删除一个 / 一段元素，返回下一个位置的迭代器。

- `pos` 必须是有效位置（不能是 `end()`）；
- 被删除的元素会被销毁，指向它们的迭代器 / 引用失效；
- 其他元素的迭代器不受影响。

## 头部与尾部

```C++
void push_back(const T& value);
void push_back(T&& value);
template<class... Args> reference emplace_back(Args&&... args);
void pop_back()noexcept;

void push_front(const T& value);
void push_front(T&& value);
template<class... Args> reference emplace_front(Args&&... args);
void pop_front()noexcept;
```

`emplace_*` 返回新元素的引用。空链表上 `pop_*` 是未定义行为。

## `resize`

```C++
void resize(size_type n);
void resize(size_type n, const T& value);
```

- 变长：在尾部补默认插入的元素 / `value` 的副本；
- 变短：从尾部删除多余元素；
- 复杂度与 `|n - 原元素数|` 成线性；
- 基本异常保证。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <iterator>
#include <ranges>
#include <vector>

int main()
{
    Yc::list_without_size<int> l;

    l.push_back(2);
    l.push_front(1);
    l.emplace_back(3);
    assert(l.front() == 1 && l.back() == 3);

    auto it = l.insert(std::next(l.begin()), 9);   // 1, 9, 2, 3
    assert(*it == 9);

    it = l.erase(std::next(l.begin()));            // 1, 2, 3
    assert(*it == 2);

    l.resize(5);                                   // 1, 2, 3, 0, 0
    assert(std::distance(l.begin(), l.end()) == 5);

    l.resize(2);                                   // 1, 2
    assert(l.back() == 2);

    l.append_range(std::views::iota(3, 5));        // 1, 2, 3, 4
    l.prepend_range(std::views::iota(0, 1));       // 0, 1, 2, 3, 4
    assert(l.front() == 0 && l.back() == 4);

    l.clear();
    assert(l.empty());
}
```
