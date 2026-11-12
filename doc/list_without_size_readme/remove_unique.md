# `Yc::list_without_size<T, Alloc>::remove` , `remove_if` , `unique`

```C++
size_type remove(const T& value);

template<class UnaryPred>
size_type remove_if(UnaryPred pred);

size_type unique();

template<class BinaryPred>
size_type unique(BinaryPred pred);
```

删除元素并返回**删除的个数**（C++20 起 `std::list` 的 `remove` / `remove_if` / `unique`
也返回 `size_type`）。

- `remove`：删除所有等于 `value` 的元素；
- `remove_if`：删除所有使 `pred(element)` 为真的元素；
- `unique`：删除相邻的重复元素，只保留每段连续重复的第一个；
- `unique(pred)`：用 `pred(前一个, 后一个)` 判断是否重复。

被删除的元素会被销毁，指向它们的迭代器 / 引用失效；其他元素不受影响。

## 参数

|||
|-:|-|
|`value`|要删除的值|
|`pred`|`remove_if`：一元谓词，接收 `const T&`<br>`unique`：二元谓词，接收两个 `const T&`|

## 复杂度

`O(n)`。

## 注意

- `unique` 只比较相邻元素；要删除所有重复值请先 [`sort`](merge_sort.md)；
- `remove(value)` 在比较时持有 `value` 的引用。如果 `value` 本身引用链内元素，且该元素会被
  删除，请先复制一份（例如 `auto v = l.front(); l.remove(v);`），否则后续比较会读到已析构的对象。

## 示例

```C++
#include "list_without_size.h"
#include <cassert>
#include <vector>

int main()
{
    Yc::list_without_size<int> l{1, 2, 2, 3, 4};

    assert(l.remove(2) == 2);
    assert((std::vector<int>(l.begin(), l.end()) == std::vector<int>{1, 3, 4}));

    assert(l.remove_if([](int v) { return v % 2 != 0; }) == 2);
    assert((std::vector<int>(l.begin(), l.end()) == std::vector<int>{4}));

    Yc::list_without_size<int> u{1, 1, 2, 2, 2, 3, 3};
    assert(u.unique() == 4);
    assert((std::vector<int>(u.begin(), u.end()) == std::vector<int>{1, 2, 3}));
}
```
