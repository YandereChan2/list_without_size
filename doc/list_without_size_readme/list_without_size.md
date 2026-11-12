# Yc::list_without_size

在文件 [`"list_without_size.h"`](../../list_without_size.h) 中定义：

```C++
template<class T, class Alloc = std::allocator<T>>
class list_without_size;
```

成员函数与 `std::list` 保持一致，**唯一不提供的是 `size()`**。链表本身不记录元素个数，
因此没有 O(1) 的 `size()` 可返回（`empty()` 仍然是 O(1)）；需要个数时用
`std::distance(l.begin(), l.end())`。

## 依赖

本容器建立在侵入式链表之上，依赖仓库
[`YandereChan2/intrusive_list`](https://github.com/YandereChan2/intrusive_list) 中的
[`intrusive_list.h`](https://github.com/YandereChan2/intrusive_list/blob/main/intrusive_list.h)：

- 每个元素放在一个
  [`intrusive_list_node<void_pointer, T>`](https://github.com/YandereChan2/intrusive_list/blob/main/doc/intrusive_list_readme/node.md)
  里，由分配器管理生命周期；
- 链表结构（哨兵、插入、摘除、`splice`、稳定排序、稳定合并、逆序）由
  [`basic_intrusive_list<void_pointer>`](https://github.com/YandereChan2/intrusive_list/blob/main/doc/intrusive_list_readme/intrusive_list.md)
  维护；
- 本类只负责三件事：分配 / 释放节点、把节点迭代器适配成 `T` 的迭代器、
  把作用于节点的谓词 / 比较器适配成作用于 `T` 的谓词 / 比较器。

上述链接指向侵入式链表仓库的文件与文档。本库**不固定依赖版本**：CMake 在配置时从
`main` 分支取最新版本，手动构建时 clone 最新仓库并把它的目录加入头文件搜索路径。

## 与 `std::list` 的差异

| | |
|:-|:-|
|`size()`|不提供。链表不记录元素个数。|
|比较运算符|不提供（`std::list` 的比较是非成员函数）。|
|`std::erase` / `std::erase_if`|不提供（同上）。|
|节点开销|每个元素一个节点：两个链接指针 + `T`。|
|其余成员函数|与 C++23 的 `std::list` 一致，包括 `insert_range` / `append_range` / `prepend_range` / `assign_range` / `from_range` 构造。|

## 成员类型

|成员类型|定义|
|-:|:-|
|`value_type`|`T`|
|`allocator_type`|`Alloc`|
|`size_type`|`std::size_t`|
|`difference_type`|`std::ptrdiff_t`|
|`reference` / `const_reference`|`T&` / `const T&`|
|`pointer` / `const_pointer`|`allocator_traits<Alloc>::pointer` / `const_pointer`|
|[`iterator`](iterators.md) / [`const_iterator`](iterators.md)|双向迭代器，解引用得到 `T&` / `const T&`|
|`reverse_iterator` / `const_reverse_iterator`|`std::reverse_iterator<...>`|

## 成员函数

| | |
|:-|:-|
|[（构造函数）](constructor.md)|默认 / 分配器 / 个数 / 个数+值 / 区间 / 拷贝 / 移动 / 带分配器的拷贝与移动 / `initializer_list` / `from_range`|
|[（析构函数）](destructor.md)|销毁全部元素|
|[`operator=`](operator_assign.md)|拷贝 / 移动 / `initializer_list` 赋值|
|[`get_allocator`](allocator.md)|返回分配器|

### 迭代器

| | |
|:-|:-|
|[`begin` / `end`](iterators.md)|首元素 / 尾后位置（含 const 版本）|
|[`cbegin` / `cend`](iterators.md)|只读版本|
|[`rbegin` / `rend` / `crbegin` / `crend`](iterators.md)|反向迭代|

### 容量

| | |
|:-|:-|
|[`empty`](capacity.md)|是否为空，O(1)|
|[`max_size`](capacity.md)|最大可能元素数|
|`size`|**不提供**|

### 元素访问

| | |
|:-|:-|
|[`front`](element_access.md)|首元素|
|[`back`](element_access.md)|尾元素|

### 修改器

| | |
|:-|:-|
|[`assign` / `assign_range`](assign.md)|替换内容|
|[`clear`](modifiers.md)|清空|
|[`insert` / `emplace` / `insert_range`](modifiers.md)|在指定位置插入|
|[`append_range` / `prepend_range`](modifiers.md)|在尾部 / 头部追加一段|
|[`erase`](modifiers.md)|删除一个 / 一段元素|
|[`push_back` / `emplace_back` / `pop_back`](modifiers.md)|尾部|
|[`push_front` / `emplace_front` / `pop_front`](modifiers.md)|头部|
|[`resize`](modifiers.md)|调整元素个数|
|[`swap`](swap.md)|交换两个链表|

### 操作

| | |
|:-|:-|
|[`splice`](splice.md)|搬移整条链 / 单个元素 / 一段元素（含右值重载）|
|[`remove` / `remove_if` / `unique`](remove_unique.md)|按值 / 谓词删除，去重；返回删除个数|
|[`merge` / `sort`](merge_sort.md)|稳定合并 / 稳定排序|
|[`reverse`](reverse.md)|逆序|

## 模板参数

|||
|-:|:-|
|`T`|元素类型。|
|`Alloc`|分配器，默认 `std::allocator<T>`。分配器给节点的指针类型决定侵入式链表的 `vptr`，因此支持缀饰指针（fancy pointer），见[分配器](allocator.md)。|

## 生命周期与失效规则

- 元素就地构造在节点里；插入 / 删除 / `splice` 只改指针，不搬移元素；
- 插入、`splice`、`sort`、`merge`、`reverse` 不会使其他元素的迭代器 / 引用失效；
- `erase` / `pop_*` / `remove*` / `unique` / `clear` 会销毁被删除的元素，指向它们的
  迭代器 / 引用随之失效；
- `swap` 不使迭代器失效（迭代器跟随元素，不跟随容器）；
- 析构析构全部元素并释放节点。

## 复杂度总览

|操作|复杂度|
|:-|:-|
|`empty` / `front` / `back` / `begin` / `end`|常数|
|`insert` / `emplace` / `erase`（单个）/ `push_*` / `pop_*` / `splice`|常数|
|`clear` / `assign` / `resize` / `remove*` / `unique` / `reverse` / 析构|线性|
|`sort`|`O(n log n)`，稳定，不分配|
|`merge`|线性，稳定，不分配|

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <iterator>
#include <vector>

int main()
{
    Yc::list_without_size<int> l{3, 1, 2};

    l.push_back(4);
    l.push_front(0);
    assert(l.front() == 0);
    assert(l.back() == 4);
    assert(!l.empty());
    assert(std::distance(l.begin(), l.end()) == 5);   // 没有 size()

    l.sort();                                          // 稳定排序
    assert((std::vector<int>(l.begin(), l.end()) == std::vector<int>{0, 1, 2, 3, 4}));

    l.reverse();
    assert(l.front() == 4);

    auto it = l.begin();
    ++it;
    it = l.erase(it);                                  // 删除 3
    assert(*it == 2);

    l.remove_if([](int v) { return v % 2 == 0; });      // 删除 4、2、0
    assert(std::distance(l.begin(), l.end()) == 1);

    l.clear();
    assert(l.empty());
}
```

与 `std::list` 相同的另一部分语义：

```C++
Yc::list_without_size<int> a{1, 2, 3};
Yc::list_without_size<int> b{4, 5};

// 拼接不分配、不搬元素，迭代器保持有效
auto keep = a.begin();          // 指向 1
a.splice(a.end(), b);
assert(b.empty());
assert(*keep == 1);

// 稳定合并（两边都必须有序）
a.sort();
Yc::list_without_size<int> c{0, 6};
c.sort();
a.merge(c);
assert(c.empty());
```

## 自检

[`test_list_without_size.cpp`](../../test_list_without_size.cpp) 覆盖了：

- 与 `std::list` 的 2000 步随机差分（插入 / 删除 / 删除区间 / `remove` / `remove_if` /
  `sort` / `reverse` / `unique` / `resize` / `splice` / `merge`）；
- 构造 / 赋值 / `assign` 的全部重载，包括 `from_range` 与 C++23 range 成员；
- 分配器传播（POCCA / POCMA / POCS）、有状态分配器、分配计数平衡；
- 异常注入：元素构造抛异常、拷贝抛异常、排序比较器抛异常时节点不丢、不泄漏；
- 缀饰指针（普通 fancy 指针与"存相对偏移"的指针）；
- 只可移动类型、单遍迭代器、空链 / 单元素、大规模排序与删除。
