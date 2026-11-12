# `Yc::list_without_size<T, Alloc>::front` , `back`

```C++
reference front()noexcept;
const_reference front()const noexcept;

reference back()noexcept;
const_reference back()const noexcept;
```

返回首元素 / 尾元素的引用。

- 空链表上调用是未定义行为；
- 返回的是元素本身（`T&`），不是迭代器；
- 常数时间。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>

int main()
{
    Yc::list_without_size<int> l{1, 2, 3};

    assert(l.front() == 1);
    assert(l.back() == 3);

    l.front() = 10;                 // 可以修改
    l.back() = 30;
    assert(l.front() == 10 && l.back() == 30);

    const auto& cl = l;
    assert(cl.front() == 10);
    assert(cl.back() == 30);
}
```

## 复杂度

常数。

## 异常

（无）
