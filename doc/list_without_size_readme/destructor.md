# `Yc::list_without_size<T, Alloc>::~list_without_size`

```C++
~list_without_size();
```

销毁全部元素并释放所有节点：对每个元素调用分配器的 `destroy`，然后销毁节点对象、
把内存还给分配器。

- 复杂度与元素个数成线性；
- 元素析构函数抛异常会导致 `std::terminate`（析构函数默认 `noexcept`）；
- 析构之后，指向其元素的迭代器 / 引用全部失效。

```C++
{
    Yc::list_without_size<int> l{1, 2, 3};
}   // 三个元素与节点在这里销毁
```

## 异常

（无）—— 析构函数为 `noexcept`。
