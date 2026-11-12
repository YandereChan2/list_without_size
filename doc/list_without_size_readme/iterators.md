# `Yc::list_without_size<T, Alloc>::begin` / `end` / `rbegin` / `rend` 等

```C++
iterator begin()noexcept;
iterator end()noexcept;
const_iterator begin()const noexcept;
const_iterator end()const noexcept;

const_iterator cbegin()const noexcept;
const_iterator cend()const noexcept;

reverse_iterator rbegin()noexcept;
reverse_iterator rend()noexcept;
const_reverse_iterator rbegin()const noexcept;
const_reverse_iterator rend()const noexcept;

const_reverse_iterator crbegin()const noexcept;
const_reverse_iterator crend()const noexcept;
```

- [`iterator` / `const_iterator`](../../list_without_size.h) 是双向迭代器：解引用得到 `T&` /
  `const T&`，支持 `++` / `--` / `==` / `!=`，`iterator` 可隐式转换为 `const_iterator`；
- `end()` 是尾后位置，不能解引用；
- 反向迭代器是 `std::reverse_iterator<...>`。

## 失效规则

迭代器只保存节点指针，因此：

- 插入、`splice`、`sort`、`merge`、`reverse` 不会使任何迭代器 / 引用失效；
- [`erase`](modifiers.md) / `pop_*` / [`remove*`](remove_unique.md) / [`unique`](remove_unique.md) /
  [`clear`](modifiers.md) 会销毁被删除的元素，指向它们的迭代器 / 引用失效；
- `swap` 不使迭代器失效：迭代器跟随元素，不跟随容器。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <iterator>
#include <vector>

int main()
{
    Yc::list_without_size<int> l{1, 2, 3};

    int sum = 0;
    for (int v : l)
    {
        sum += v;
    }
    assert(sum == 6);

    // 反向
    std::vector<int> r(l.rbegin(), l.rend());
    assert((r == std::vector<int>{3, 2, 1}));

    // iterator -> const_iterator
    Yc::list_without_size<int>::const_iterator ci = l.begin();
    assert(ci == l.begin());
    assert(l.begin() == ci);

    // const 视图
    const auto& cl = l;
    assert(std::distance(cl.begin(), cl.end()) == 3);

    // 迭代器稳定性
    auto keep = l.begin();
    l.push_front(0);
    l.push_back(4);
    assert(*keep == 1);
}
```
