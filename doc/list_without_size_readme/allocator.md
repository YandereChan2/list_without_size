# 分配器

```C++
using allocator_type = Alloc;
allocator_type get_allocator()const noexcept;
```

## 节点分配

元素放在
[`intrusive_list_node<void_pointer, T>`](https://github.com/YandereChan2/intrusive_list/blob/main/doc/intrusive_list_readme/node.md)
里。节点由 `Alloc` 重绑定后的 `rebind_alloc<node_type>` 分配；`T` 的构造 / 析构通过原分配器
（`allocator_traits<Alloc>::construct` / `destroy`）完成。

## 传播语义

|traits|行为|
|:-|:-|
|`propagate_on_container_copy_assignment`|拷贝赋值时用 `other` 的分配器替换（先释放旧节点，再更换）|
|`propagate_on_container_move_assignment`|移动赋值时接管 `other` 的分配器与节点（O(1)）|
|`propagate_on_container_swap`|`swap` 时交换分配器与节点|
|`is_always_equal`|移动构造 / 移动赋值 / `swap` 的 `noexcept` 依据|

不传播且分配器不相等时，移动构造 / 移动赋值退化为逐元素移动。

`splice` / `merge` 要求两个链表的分配器相等（与 `std::list` 相同）。不相等时标准规定为
未定义行为；本库在 Debug 下用 `assert` 捕获。

## 空分配器与对象大小

空分配器通过继承做 EBO（空基类优化）。因此：

```C++
sizeof(Yc::list_without_size<int>);   // 一个哨兵：两个指针
```

（`[[no_unique_address]]` 在部分编译器上对模板参数不生效，所以用继承。）

## 缀饰指针

`void_pointer = allocator_traits<Alloc>::void_pointer` 决定侵入式链表的 `vptr`：

- 普通分配器（`std::allocator<T>`）：`void*`；
- 缀饰指针分配器：对应的 `fancy<void>`，节点、哨兵、迭代器整体使用该指针。

分配器对节点的 `pointer` 必须等于 `pointer_traits<Alloc::void_pointer>::rebind<节点类型>`；
不满足时编译期 `static_assert` 报错。

> 注意：如果缀饰指针是"自相对"的（拷贝构造的结果依赖对象自身地址，例如
> `boost::interprocess::offset_ptr` 一类实现），MSVC 14.5x 在 `/O2` 下内联其
> 拷贝构造 / 赋值时存在代码生成缺陷。本库的测试对这类指针禁用了相关成员的内联，
> 见 [`test_list_without_size.cpp`](../../test_list_without_size.cpp)。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <cstddef>
#include <memory>

template<class T>
struct id_allocator
{
    using value_type = T;
    int id = 0;

    id_allocator() = default;
    explicit id_allocator(int i) : id(i) {}
    template<class U>
    id_allocator(const id_allocator<U>& o) noexcept : id(o.id) {}

    T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) noexcept { std::allocator<T>{}.deallocate(p, n); }

    friend bool operator==(const id_allocator& a, const id_allocator& b) noexcept
    {
        return a.id == b.id;
    }
};

int main()
{
    using list = Yc::list_without_size<int, id_allocator<int>>;

    list a(3, 4, id_allocator<int>{1});
    assert(a.get_allocator().id == 1);

    list b(a, id_allocator<int>{2});       // 拷贝 + 显式分配器
    assert(b.get_allocator().id == 2);

    b = a;                                 // id_allocator 不传播分配器
    assert(b.get_allocator().id == 2);
}
```

完整用例（传播、逐元素移动、`swap`）见
[`test_list_without_size.cpp`](../../test_list_without_size.cpp)。
