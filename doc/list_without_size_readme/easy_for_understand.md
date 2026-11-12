# `list_without_size` 的设计说明

本文解释这个容器为什么长这样：

- 为什么没有 `size()`；
- 它和侵入式链表的分工；
- 迭代器是怎么适配出来的；
- `assign` 为什么先覆写再补插；
- 异常安全是怎么保证的；
- 分配器与缀饰指针如何一路传到底层。

## 为什么没有 `size()`

`std::list` 从 C++11 起要求 O(1) 的 `size()`，实现方式是在容器里多存一个计数器，
每次插入 / 删除都维护它。

本容器选择不记录个数：

- 链表结构完全由侵入式链表维护，它不数元素；
- 省掉计数器后，插入 / 删除 / `splice` 只改指针，不必同步更新任何计数；
- 代价是没有 O(1) 的 `size()`——所以干脆不提供它。

需要个数时用 `std::distance(l.begin(), l.end())`，它是 O(n) 的。`empty()` 仍然是 O(1)：
判断哨兵是否脱管即可。

## 与侵入式链表的分工

```
list_without_size<T, Alloc>                 拥有元素、分配器、std::list 接口
        │
        ├── basic_intrusive_list<void_pointer>    哨兵 + 链接操作（sort/merge/splice/reverse）
        │
        └── intrusive_list_node<void_pointer, T>  就地存放 T 的节点
```

- 节点由 `Alloc` 重绑定后的分配器分配；
- `T` 的构造 / 析构走分配器的 `construct` / `destroy`（`intrusive_list_node::construct_value`
  / `destroy_value`）；
- 插入、摘除、拼接、排序、合并、逆序全部委托给 `basic_intrusive_list`；
- 本类只做"适配"：节点迭代器 → 元素迭代器，节点比较器 → 元素比较器，节点回收器 → 元素回收器。

这样侵入式层不需要知道 `T`，拥有型层不需要写链表算法。

## 迭代器适配

侵入式链表的迭代器解引用得到**节点**；`std::list` 的迭代器必须解引用得到 **`T&`**。
`details::list_without_size_iterator<T, vptr, Const>` 因此只做一件事：

```C++
reference operator*()const noexcept
{
    return static_cast<intrusive_node&>(*cur).value();   // 节点 -> T&
}
```

其余行为（`++` / `--` / 比较 / `iterator` → `const_iterator` 转换）原样转发给底层迭代器。
迭代器只保存节点指针，所以：

- 只要元素还在链表里，迭代器就有效；
- 插入 / `splice` / `sort` / `merge` / `reverse` 不会让其他迭代器失效。

## 节点回收器

侵入式链表的 `remove_if` / `unique` 只摘链、不销毁节点，并要求调用方提供一个回收器。
本类传的是：

```C++
[this](node_base& n) { destroy_node(as_node(n)); }
```

`destroy_node` 依次做三件事：`destroy_value`（析构 `T`）、销毁节点对象、把内存还给分配器。
因此对使用者来说，`remove_if` / `unique` 就是"删除元素"。

## `assign` 为什么先覆写再补插

朴素实现是"先 `clear()` 再插入"，但如果实参引用了链内元素：

```C++
Yc::list_without_size<int> l{1, 2, 3};
l.assign(4, l.front());     // 实参是链内元素
```

先 `clear()` 会把 `l.front()` 销毁，后面的复制就悬空了。`std::list` 的 `assign` 也是
"先覆写已有元素，再补插或截断"：

1. 逐个把已有元素赋值为 `value`；
2. 元素不够时在尾部补插；
3. 元素多了就删掉尾部。

这样实参在整个过程中一直存活。

## 异常安全

- 单个 `insert` / `emplace` / `push_*`：先构造节点，成功后才挂链；构造失败时链表不变
  （强保证）。
- 构造 / 拷贝构造 / 区间插入：中途抛异常时，已插入的节点会被回收（构造函数里
  `catch (...) { clear(); throw; }`），已有元素不丢，也不泄漏。
- `resize` / `assign`：基本保证（已完成的修改保留）。
- `sort`：比较器抛异常时，侵入式链表会把拆出去的节点全部拼回，元素一个不丢。

## 分配器与缀饰指针

```C++
using void_pointer = typename std::allocator_traits<Alloc>::void_pointer;
using node_type = intrusive_list_node<void_pointer, T>;
using list_type = basic_intrusive_list<void_pointer>;
```

`vptr` 直接取自分配器的 `void_pointer`。于是：

- 普通分配器（`std::allocator<T>`）得到 `void*`，节点指针就是普通指针；
- 缀饰指针分配器得到对应的 `fancy<void>`，节点、哨兵、迭代器整体换成缀饰指针。

分配器语义与 `std::list` 相同：`POCCA` / `POCMA` / `POCS` 三条传播路径、
分配器不相等时的逐元素移动、`select_on_container_copy_construction` 都有实现。
空分配器用继承做 EBO，`list_without_size<int>` 对象只占两个指针。

## 与 `std::list` 的取舍

| | `std::list` | `list_without_size` |
|:-|:-|:-|
|`size()`|O(1)|不提供|
|`splice`|O(1)|O(1)|
|插入 / 删除|O(1)|O(1)|
|元素地址|稳定|稳定|
|迭代器失效|仅被删元素|仅被删元素|

接口上，除 `size()` 之外与 C++23 的 `std::list` 对齐（包括 range 成员与 `from_range` 构造）；
比较运算符、`std::erase` / `std::erase_if` 是非成员接口，本库不提供。
