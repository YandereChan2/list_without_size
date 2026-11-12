// 测试的全部检查都在 assert 里；Release（NDEBUG）下也要生效，否则等于不测
#ifdef NDEBUG
#  undef NDEBUG
#endif
#include "list_without_size.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <random>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using Yc::list_without_size;

// ---------------------------------------------------------------------------
// 测试用类型与工具
// ---------------------------------------------------------------------------

// 统计构造/析构次数：任何路径下都不允许泄漏或重复析构
struct track
{
    static inline int alive = 0;
    int v = 0;

    track() { ++alive; }
    explicit track(int x) : v(x) { ++alive; }
    track(const track& o) : v(o.v) { ++alive; }
    track(track&& o) noexcept : v(o.v) { o.v = -1; ++alive; }
    track& operator=(const track& o) { v = o.v; return *this; }
    track& operator=(track&& o) noexcept { v = o.v; o.v = -1; return *this; }
    ~track() { --alive; }

    friend bool operator==(const track& a, const track& b) { return a.v == b.v; }
    friend bool operator<(const track& a, const track& b) { return a.v < b.v; }
};

// 排序/合并稳定性用：同 key 时看 id 的先后
struct rec
{
    int key = 0;
    int id = 0;
};

template<class C>
static std::vector<int> vals(const C& c)
{
    std::vector<int> out;
    for (auto it = c.begin(); it != c.end(); ++it)
    {
        out.push_back(*it);
    }
    return out;
}

template<class C>
static std::vector<int> keys(const C& c)
{
    std::vector<int> out;
    for (auto it = c.begin(); it != c.end(); ++it)
    {
        out.push_back(it->key);
    }
    return out;
}

template<class C>
static std::vector<int> ids(const C& c)
{
    std::vector<int> out;
    for (auto it = c.begin(); it != c.end(); ++it)
    {
        out.push_back(it->id);
    }
    return out;
}

template<class C>
static void expect_vals(const C& c, std::initializer_list<int> il)
{
    assert(vals(c) == std::vector<int>(il));
}

template<class C>
static void expect_keys(const C& c, std::initializer_list<int> il)
{
    assert(keys(c) == std::vector<int>(il));
}

template<class C>
static void expect_ids(const C& c, std::initializer_list<int> il)
{
    assert(ids(c) == std::vector<int>(il));
}

// 统计分配次数：验证每次 allocate 都有对应的 deallocate
struct alloc_stats
{
    static inline std::size_t allocated = 0;
    static inline std::size_t deallocated = 0;
    static void reset() { allocated = deallocated = 0; }
    static bool balanced() { return allocated == deallocated; }
    static std::size_t outstanding() { return allocated - deallocated; }
};

template<class T>
struct counting_allocator
{
    using value_type = T;
    counting_allocator() = default;
    template<class U>
    counting_allocator(const counting_allocator<U>&) noexcept {}

    T* allocate(std::size_t n)
    {
        ++alloc_stats::allocated;
        return std::allocator<T>{}.allocate(n);
    }
    void deallocate(T* p, std::size_t n) noexcept
    {
        ++alloc_stats::deallocated;
        std::allocator<T>{}.deallocate(p, n);
    }

    friend bool operator==(counting_allocator, counting_allocator) noexcept = default;
};

// 有状态分配器：验证 get_allocator / 带分配器的构造 / 分配器不相等时的移动
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

    friend bool operator==(const id_allocator& a, const id_allocator& b) noexcept { return a.id == b.id; }
};

// 三件套都传播的有状态分配器：覆盖 POCCA/POCMA/POCS 为 true 的分支
template<class T>
struct prop_allocator
{
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    int id = 0;

    prop_allocator() = default;
    explicit prop_allocator(int i) : id(i) {}
    template<class U>
    prop_allocator(const prop_allocator<U>& o) noexcept : id(o.id) {}

    T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) noexcept { std::allocator<T>{}.deallocate(p, n); }

    friend bool operator==(const prop_allocator& a, const prop_allocator& b) noexcept { return a.id == b.id; }
};

// 极简缀饰指针：只实现 pointer_traits 与侵入式链表需要的那几件事
template<class T>
class fancy_ptr
{
    T* p_ = nullptr;
public:
    using element_type = T;
    using difference_type = std::ptrdiff_t;
    template<class U>
    using rebind = fancy_ptr<U>;

    fancy_ptr() = default;
    explicit fancy_ptr(T* p) noexcept : p_(p) {}
    // 限定转换：fancy_ptr<U> -> fancy_ptr<const U> 等
    template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
    fancy_ptr(const fancy_ptr<U>& other) noexcept : p_(other.get()) {}

    T* get() const noexcept { return p_; }
    T& operator*() const noexcept { return *p_; }
    T* operator->() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }

    fancy_ptr& operator++() noexcept { ++p_; return *this; }
    fancy_ptr& operator--() noexcept { --p_; return *this; }

    static fancy_ptr pointer_to(T& r) noexcept { return fancy_ptr(std::addressof(r)); }

    friend bool operator==(fancy_ptr, fancy_ptr) = default;
};

// void 版本只用来做"节点指针"的 rebind 源，不需要解引用能力
template<>
class fancy_ptr<void>
{
public:
    using element_type = void;
    using difference_type = std::ptrdiff_t;
    template<class U>
    using rebind = fancy_ptr<U>;
};

// 返回缀饰指针的分配器
template<class T>
struct fancy_allocator
{
    using value_type = T;
    using pointer = fancy_ptr<T>;
    using size_type = std::size_t;

    fancy_allocator() = default;
    template<class U>
    fancy_allocator(const fancy_allocator<U>&) noexcept {}

    pointer allocate(std::size_t n)
    {
        return pointer{ std::allocator<T>{}.allocate(n) };
    }
    void deallocate(pointer p, std::size_t n) noexcept
    {
        std::allocator<T>{}.deallocate(p.get(), n);
    }

    friend bool operator==(fancy_allocator, fancy_allocator) noexcept = default;
};

// 更"真实"的缀饰指针：内部存的是"相对自身地址的偏移"。一旦实现里把缀饰指针
// 偷偷换成原始地址（或在拷贝/移动时按位复制），结果就会错。
//
// 注意：MSVC 14.5x 在 /O2 下内联"拷贝构造依赖 this 地址"的类型时会生成错误代码
// （把复制当成按位复制），所以这里对这类成员禁用内联，以继续在 Release 下验证
// 自相对指针。指针类型本身的语义与 boost::interprocess::offset_ptr 一致。
#if defined(_MSC_VER)
#  define YC_TEST_OFFSET_PTR_NOINLINE __declspec(noinline)
#else
#  define YC_TEST_OFFSET_PTR_NOINLINE
#endif

template<class T>
class offset_ptr
{
    // 偏移 0 是合法值（指向自身），所以空指针另用一个不可能出现的偏移 1
    static constexpr std::ptrdiff_t null_offset = 1;
    std::ptrdiff_t offset_ = null_offset;

    char* self() const noexcept { return const_cast<char*>(reinterpret_cast<const char*>(this)); }
    static std::ptrdiff_t diff(const void* p, const void* base) noexcept
    {
        return p ? static_cast<const char*>(p) - static_cast<const char*>(base) : null_offset;
    }
public:
    using element_type = T;
    using difference_type = std::ptrdiff_t;
    template<class U>
    using rebind = offset_ptr<U>;

    offset_ptr() = default;
    explicit offset_ptr(T* p) noexcept : offset_(diff(p, this)) {}
    YC_TEST_OFFSET_PTR_NOINLINE offset_ptr(const offset_ptr& o) noexcept
        : offset_(diff(o.get(), this)) {}
    template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
    YC_TEST_OFFSET_PTR_NOINLINE offset_ptr(const offset_ptr<U>& o) noexcept
        : offset_(diff(o.get(), this)) {}
    YC_TEST_OFFSET_PTR_NOINLINE offset_ptr& operator=(const offset_ptr& o) noexcept
    {
        offset_ = diff(o.get(), this);
        return *this;
    }

    T* get() const noexcept
    {
        return offset_ == null_offset ? nullptr : static_cast<T*>(static_cast<void*>(self() + offset_));
    }
    T& operator*() const noexcept { return *get(); }
    T* operator->() const noexcept { return get(); }
    explicit operator bool() const noexcept { return offset_ != null_offset; }

    offset_ptr& operator++() noexcept
    {
        T* p = get();
        offset_ = p ? diff(p + 1, this) : null_offset;
        return *this;
    }
    offset_ptr& operator--() noexcept
    {
        T* p = get();
        offset_ = p ? diff(p - 1, this) : null_offset;
        return *this;
    }

    static offset_ptr pointer_to(T& r) noexcept { return offset_ptr(std::addressof(r)); }

    friend bool operator==(const offset_ptr& a, const offset_ptr& b) noexcept
    {
        return a.get() == b.get();
    }
};

template<>
class offset_ptr<void>
{
public:
    using element_type = void;
    using difference_type = std::ptrdiff_t;
    template<class U>
    using rebind = offset_ptr<U>;
};

template<class T>
struct offset_allocator
{
    using value_type = T;
    using pointer = offset_ptr<T>;
    using size_type = std::size_t;

    offset_allocator() = default;
    template<class U>
    offset_allocator(const offset_allocator<U>&) noexcept {}

    pointer allocate(std::size_t n)
    {
        return pointer{ std::allocator<T>{}.allocate(n) };
    }
    void deallocate(pointer p, std::size_t n) noexcept
    {
        std::allocator<T>{}.deallocate(p.get(), n);
    }

    friend bool operator==(offset_allocator, offset_allocator) noexcept = default;
};

// 拷贝到第 N 次时抛异常：验证 insert/构造的异常路径不泄漏节点
struct throws_on_copy
{
    static inline int alive = 0;
    static inline int countdown = -1;       // <0 不抛；==0 抛；>0 递减

    int v = 0;

    throws_on_copy(int x = 0) : v(x) { ++alive; }
    throws_on_copy(const throws_on_copy& o) : v(o.v)
    {
        if (countdown == 0)
        {
            throw std::runtime_error("copy boom");
        }
        if (countdown > 0)
        {
            --countdown;
        }
        ++alive;
    }
    throws_on_copy(throws_on_copy&& o) noexcept : v(o.v) { o.v = -1; ++alive; }
    throws_on_copy& operator=(const throws_on_copy&) = default;
    ~throws_on_copy() { --alive; }
};

// 元素构造抛异常：验证 create_node 会把半成品节点归还
struct ctor_throws
{
    static inline int alive = 0;
    explicit ctor_throws(int v)
    {
        if (v == 13)
        {
            throw std::runtime_error("ctor boom");
        }
        ++alive;
    }
    ctor_throws(const ctor_throws&) = default;
    ctor_throws(ctor_throws&&) = default;
    ~ctor_throws() { --alive; }
};

// ---------------------------------------------------------------------------
// 编译期检查：确实没有 size()，迭代器是双向迭代器
// ---------------------------------------------------------------------------
template<class C>
concept has_size_member = requires(C c) { c.size(); };

static_assert(!has_size_member<list_without_size<int>>);
static_assert(std::bidirectional_iterator<list_without_size<int>::iterator>);
static_assert(std::bidirectional_iterator<list_without_size<int>::const_iterator>);
static_assert(std::is_same_v<std::iterator_traits<list_without_size<int>::iterator>::value_type, int>);
static_assert(std::is_same_v<std::iterator_traits<list_without_size<int>::iterator>::reference, int&>);
static_assert(std::is_same_v<std::iterator_traits<list_without_size<int>::const_iterator>::reference, const int&>);
static_assert(std::ranges::bidirectional_range<list_without_size<int>&>);
static_assert(!std::ranges::sized_range<list_without_size<int>>);

// ---------------------------------------------------------------------------
// 基本操作 / 迭代器
// ---------------------------------------------------------------------------
static void test_basic()
{
    list_without_size<int> l;
    assert(l.empty());
    assert(l.begin() == l.end());
    assert(l.cbegin() == l.cend());
    assert(l.max_size() > 0);

    for (int i = 0; i != 5; ++i)
    {
        l.push_back(i);
    }
    assert(!l.empty());
    assert(l.front() == 0);
    assert(l.back() == 4);
    expect_vals(l, { 0, 1, 2, 3, 4 });
    assert(std::distance(l.begin(), l.end()) == 5);
    assert(std::ranges::distance(l) == 5);

    // 前置/后置 ++/--
    auto it = l.begin();
    assert(*it == 0);
    ++it;
    assert(*it == 1);
    auto old = it++;
    assert(*old == 1);
    assert(*it == 2);
    old = it--;
    assert(*old == 2);
    assert(*it == 1);
    --it;
    assert(*it == 0);

    // operator->
    struct kv { int k; };
    list_without_size<kv> kl;
    kl.push_back(kv{ 42 });
    assert(kl.begin()->k == 42);

    // range-for
    int sum = 0;
    for (int v : l)
    {
        sum += v;
    }
    assert(sum == 10);

    // const 视图：const_iterator、转换、跨 const 比较
    const list_without_size<int>& cl = l;
    static_assert(std::is_same_v<decltype(cl.begin()), list_without_size<int>::const_iterator>);
    assert(std::distance(cl.begin(), cl.end()) == 5);
    assert(cl.front() == 0);
    assert(cl.back() == 4);
    list_without_size<int>::const_iterator ci = l.begin();
    assert(ci == l.begin());
    assert(l.begin() == ci);
    assert(ci == cl.begin());
    int csum = 0;
    for (int v : cl)
    {
        csum += v;
    }
    assert(csum == 10);

    // 反向迭代
    assert(*l.rbegin() == 4);
    assert(*l.crbegin() == 4);
    expect_vals(l, { 0, 1, 2, 3, 4 });
    std::vector<int> rv;
    for (auto rit = l.rbegin(); rit != l.rend(); ++rit)
    {
        rv.push_back(*rit);
    }
    assert((rv == std::vector<int>{ 4, 3, 2, 1, 0 }));
    assert(std::distance(l.rbegin(), l.rend()) == 5);

    // push_front / pop_front / pop_back
    l.push_front(-1);
    l.push_front(-2);
    expect_vals(l, { -2, -1, 0, 1, 2, 3, 4 });
    l.pop_front();
    l.pop_back();
    expect_vals(l, { -1, 0, 1, 2, 3 });

    l.clear();
    assert(l.empty());
    assert(l.begin() == l.end());

    list_without_size<track> tl;
    tl.emplace_back(1);
    tl.emplace_back(2);
    assert(track::alive == 2);
    tl.clear();
    assert(track::alive == 0);

    // 与标准算法/容器互操作
    list_without_size<int> il{ 3, 1, 4, 1, 5 };
    std::vector<int> vv(il.begin(), il.end());
    assert((vv == std::vector<int>{ 3, 1, 4, 1, 5 }));
    std::list<int> sl(il.begin(), il.end());
    assert(std::equal(sl.begin(), sl.end(), il.begin(), il.end()));
    assert(std::equal(il.begin(), il.end(), sl.begin(), sl.end()));
    assert(std::find(il.begin(), il.end(), 4) != il.end());
    assert(std::count(il.begin(), il.end(), 1) == 2);
    assert(std::accumulate(il.begin(), il.end(), 0) == 14);
    assert(std::distance(il.cbegin(), il.cend()) == 5);
}

// ---------------------------------------------------------------------------
// 构造 / 赋值
// ---------------------------------------------------------------------------
static void test_construct_assign()
{
    // (n, value) / (n)
    list_without_size<int> a(3, 7);
    expect_vals(a, { 7, 7, 7 });
    list_without_size<int> z(4);
    expect_vals(z, { 0, 0, 0, 0 });
    assert(list_without_size<int>(0, 9).empty());

    // 迭代器区间
    std::vector<int> src{ 1, 2, 3, 4 };
    list_without_size<int> b(src.begin(), src.end());
    expect_vals(b, { 1, 2, 3, 4 });
    list_without_size<int> b2(src.begin(), src.begin() + 2);
    expect_vals(b2, { 1, 2 });

    // initializer_list / from_range
    list_without_size<int> c{ 5, 6, 7 };
    expect_vals(c, { 5, 6, 7 });
    list_without_size<int> c2(std::from_range, std::views::iota(8, 11));
    expect_vals(c2, { 8, 9, 10 });

    // 拷贝 / 移动
    list_without_size<int> d(c);
    expect_vals(d, { 5, 6, 7 });
    d.push_back(8);
    expect_vals(c, { 5, 6, 7 });

    list_without_size<int> e(std::move(d));
    expect_vals(e, { 5, 6, 7, 8 });
    assert(d.empty());

    // 带分配器的构造
    using ia = list_without_size<int, id_allocator<int>>;
    ia s1(3, 4, id_allocator<int>{ 1 });
    expect_vals(s1, { 4, 4, 4 });
    assert(s1.get_allocator().id == 1);
    ia s2(id_allocator<int>{ 2 });
    assert(s2.get_allocator().id == 2);
    ia s3(s1, id_allocator<int>{ 3 });                  // 拷贝 + 换分配器
    expect_vals(s3, { 4, 4, 4 });
    assert(s3.get_allocator().id == 3);

    // 移动构造：分配器相等时整体接管
    ia s4(std::move(s1), id_allocator<int>{ 1 });
    expect_vals(s4, { 4, 4, 4 });
    assert(s1.empty());

    // 移动构造：分配器不相等时逐元素移动
    ia s5(std::move(s4), id_allocator<int>{ 9 });
    expect_vals(s5, { 4, 4, 4 });
    assert(s4.empty());
    assert(s5.get_allocator().id == 9);

    // initializer_list + 分配器
    ia s6({ 1, 2 }, id_allocator<int>{ 7 });
    expect_vals(s6, { 1, 2 });
    assert(s6.get_allocator().id == 7);

    // 赋值：拷贝 / 移动 / init-list / assign
    list_without_size<int> f{ 1, 2, 3 };
    list_without_size<int> g{ 9 };
    f = g;
    expect_vals(f, { 9 });
    f = list_without_size<int>{ 4, 5 };
    expect_vals(f, { 4, 5 });
    assert(g.empty() == false);
    f = { 6, 7, 8 };
    expect_vals(f, { 6, 7, 8 });
    f.assign(2, 1);
    expect_vals(f, { 1, 1 });
    f.assign(src.begin(), src.end());
    expect_vals(f, { 1, 2, 3, 4 });
    f.assign({ 0, 1 });
    expect_vals(f, { 0, 1 });
    f.assign_range(std::views::iota(5, 8));
    expect_vals(f, { 5, 6, 7 });

    // assign 的 value 引用链内元素（不能悬空）
    list_without_size<track> t1;
    t1.emplace_back(1);
    t1.emplace_back(2);
    const track& ref = t1.back();
    t1.assign(3, ref);                     // 值来自链内元素
    assert(t1.begin()->v == 2);
    assert(std::distance(t1.begin(), t1.end()) == 3);
    t1.assign(1, ref);                     // 缩短也要安全
    assert(std::distance(t1.begin(), t1.end()) == 1);
    assert(t1.begin()->v == 2);
    t1.clear();
    assert(track::alive == 0);

    // 分配器不平衡就说明泄漏了
    alloc_stats::reset();
    {
        using ca = list_without_size<int, counting_allocator<int>>;
        ca h{ 1, 2, 3 };
        h.push_back(4);
        h.insert(h.begin(), 0);
        h.pop_back();
        h.erase(h.begin());
        ca h2(h);
        h = h2;
        h.clear();
    }
    assert(alloc_stats::balanced());
}

// ---------------------------------------------------------------------------
// insert / emplace / erase / resize
// ---------------------------------------------------------------------------
static void test_modifiers()
{
    list_without_size<int> l;
    auto it = l.insert(l.end(), 1);
    assert(*it == 1);
    it = l.insert(l.end(), 2);
    it = l.insert(it, 3);                   // 在 2 前插 3
    expect_vals(l, { 1, 3, 2 });
    assert(*it == 3);

    it = l.insert(l.begin(), 2, 0);         // 头部插两个 0
    assert(*it == 0);
    expect_vals(l, { 0, 0, 1, 3, 2 });

    it = l.emplace(l.end(), 9);
    assert(*it == 9);
    assert(l.back() == 9);

    std::vector<int> more{ 7, 8 };
    it = l.insert(l.begin(), more.begin(), more.end());
    assert(*it == 7);
    expect_vals(l, { 7, 8, 0, 0, 1, 3, 2, 9 });

    it = l.insert(l.begin(), { -1, -2 });
    assert(*it == -1);
    expect_vals(l, { -1, -2, 7, 8, 0, 0, 1, 3, 2, 9 });

    // append_range / prepend_range
    list_without_size<int> ar{ 1 };
    ar.append_range(std::views::iota(2, 4));
    ar.prepend_range(std::views::iota(0, 1));
    expect_vals(ar, { 0, 1, 2, 3 });

    it = l.insert_range(l.begin(), std::views::iota(100, 102));
    assert(*it == 100);
    expect_vals(l, { 100, 101, -1, -2, 7, 8, 0, 0, 1, 3, 2, 9 });

    // erase 单个 / 区间
    auto e = l.begin();
    ++e;
    e = l.erase(e);
    assert(*e == -1);
    expect_vals(l, { 100, -1, -2, 7, 8, 0, 0, 1, 3, 2, 9 });

    auto first = l.begin();
    auto last = first;
    for (int i = 0; i != 3; ++i)
    {
        ++last;
    }
    e = l.erase(first, last);
    assert(*e == 7);
    expect_vals(l, { 7, 8, 0, 0, 1, 3, 2, 9 });

    // 空区间 erase 返回 last
    e = l.erase(l.begin(), l.begin());
    assert(*e == 7);

    // emplace_front / emplace_back
    assert(l.emplace_front(41) == 41);
    assert(l.emplace_back(42) == 42);
    assert(l.front() == 41);
    assert(l.back() == 42);

    // resize 增/减/不变
    list_without_size<int> r{ 1, 2, 3 };
    r.resize(5);
    expect_vals(r, { 1, 2, 3, 0, 0 });
    r.resize(2);
    expect_vals(r, { 1, 2 });
    r.resize(2, 9);
    expect_vals(r, { 1, 2 });
    r.resize(4, 9);
    expect_vals(r, { 1, 2, 9, 9 });
    r.resize(0);
    assert(r.empty());
    r.resize(0);
    assert(r.empty());

    // 空链表上的 erase/insert 边界
    list_without_size<int> empty;
    assert(empty.erase(empty.begin(), empty.end()) == empty.end());
    empty.push_back(1);
    assert(empty.erase(empty.begin()) == empty.end());
    assert(empty.empty());

    // 迭代器稳定性：插入/删除其他元素不影响它
    list_without_size<int> s{ 1, 2, 3, 4 };
    auto keep = s.begin();
    ++keep;                                 // 指向 2
    s.push_front(0);
    s.push_back(5);
    s.erase(s.begin());
    s.insert(s.end(), 6);
    assert(*keep == 2);
}

// ---------------------------------------------------------------------------
// splice
// ---------------------------------------------------------------------------
static void test_splice()
{
    // 整个链表
    list_without_size<int> a{ 1, 2, 3 };
    list_without_size<int> b{ 4, 5 };
    auto keep = a.begin();                  // 指向 1
    a.splice(a.end(), b);
    assert(b.empty());
    expect_vals(a, { 1, 2, 3, 4, 5 });
    assert(*keep == 1);

    // 单元素：搬到表头
    auto it = a.begin();
    ++it;                                   // 指向 2
    a.splice(a.begin(), a, it);
    expect_vals(a, { 2, 1, 3, 4, 5 });
    assert(*it == 2);                       // 节点没变，位置变了

    // 单元素：搬到自己后面 = 无效果
    auto x = a.begin();
    auto nx = x;
    ++nx;
    a.splice(nx, a, x);
    expect_vals(a, { 2, 1, 3, 4, 5 });
    // 单元素：pos == it = 无效果
    a.splice(a.begin(), a, a.begin());
    expect_vals(a, { 2, 1, 3, 4, 5 });

    // 范围
    list_without_size<int> c{ 7, 8, 9 };
    auto cf = c.begin();
    auto cl = c.end();
    a.splice(a.end(), c, cf, cl);
    assert(c.empty());
    expect_vals(a, { 2, 1, 3, 4, 5, 7, 8, 9 });

    // 空范围 = 无效果
    a.splice(a.begin(), c, c.begin(), c.end());
    expect_vals(a, { 2, 1, 3, 4, 5, 7, 8, 9 });

    // 把范围搬到同一条链的中间（2,1,3,4 → 1,3,4 前插 2? 这里把 [7,8) 搬到 3 前）
    auto pos = a.begin();
    ++pos;
    ++pos;                                  // 指向 3
    auto f7 = a.begin();
    for (int i = 0; i != 5; ++i)
    {
        ++f7;                               // 指向 7
    }
    auto l8 = f7;
    ++l8;                                   // 指向 8（即 [7,8)）
    a.splice(pos, a, f7, l8);
    expect_vals(a, { 2, 1, 7, 3, 4, 5, 8, 9 });

    // 自 splice 整条链 = 无效果
    a.splice(a.begin(), a);
    expect_vals(a, { 2, 1, 7, 3, 4, 5, 8, 9 });

    // rvalue 重载
    list_without_size<int> d{ 10, 11 };
    auto dit = d.begin();
    a.splice(a.end(), std::move(d));
    assert(d.empty());
    expect_vals(a, { 2, 1, 7, 3, 4, 5, 8, 9, 10, 11 });

    list_without_size<int> e{ 12, 13 };
    auto eit = e.begin();
    a.splice(a.begin(), std::move(e), eit);
    assert(e.empty() == false);
    expect_vals(a, { 12, 2, 1, 7, 3, 4, 5, 8, 9, 10, 11 });
    expect_vals(e, { 13 });

    list_without_size<int> f{ 14, 15 };
    auto ff = f.begin();
    auto fl = f.end();
    a.splice(a.end(), std::move(f), ff, fl);
    assert(f.empty());
    expect_vals(a, { 12, 2, 1, 7, 3, 4, 5, 8, 9, 10, 11, 14, 15 });
}

// ---------------------------------------------------------------------------
// remove / remove_if / unique
// ---------------------------------------------------------------------------
static void test_remove_unique()
{
    list_without_size<int> l{ 1, 2, 2, 3, 2, 4 };
    auto n = l.remove(2);
    assert(n == 3);
    expect_vals(l, { 1, 3, 4 });
    assert(l.remove(42) == 0);

    n = l.remove_if([](int v) { return v % 2 != 0; });
    assert(n == 2);
    expect_vals(l, { 4 });

    list_without_size<int> u{ 1, 1, 2, 2, 2, 3, 3 };
    assert(u.unique() == 4);
    expect_vals(u, { 1, 2, 3 });
    assert(u.unique() == 0);

    list_without_size<int> u2{ 1, 3, 2, 4, 5 };
    // 相邻差值 <= 1 的合并，验证自定义谓词
    auto same_parity = [](int a, int b) { return (a % 2) == (b % 2); };
    auto r = u2.unique(same_parity);
    assert(r == 2);                          // 1,3 合并；2,4 合并；留下 1,2,5
    expect_vals(u2, { 1, 2, 5 });

    // 删除后节点确实被销毁
    list_without_size<track> t;
    t.emplace_back(1);
    t.emplace_back(1);
    t.emplace_back(2);
    assert(track::alive == 3);
    assert(t.remove_if([](const track& x) { return x.v == 1; }) == 2);
    assert(track::alive == 1);
    t.clear();
    assert(track::alive == 0);

    // 空链表
    list_without_size<int> empty;
    assert(empty.remove(1) == 0);
    assert(empty.remove_if([](int) { return true; }) == 0);
    assert(empty.unique() == 0);
}

// ---------------------------------------------------------------------------
// sort / merge / reverse
// ---------------------------------------------------------------------------
static void test_sort_merge_reverse()
{
    // sort 默认 <
    list_without_size<int> l{ 5, 3, 1, 4, 2 };
    l.sort();
    expect_vals(l, { 1, 2, 3, 4, 5 });

    list_without_size<int> empty;
    empty.sort();
    list_without_size<int> one{ 1 };
    one.sort();
    expect_vals(one, { 1 });

    // 稳定性：相同 key 保持原有先后
    auto by_key = [](const rec& a, const rec& b) { return a.key < b.key; };
    list_without_size<rec> r{ { 2, 0 }, { 1, 1 }, { 2, 2 }, { 1, 3 }, { 3, 4 } };
    r.sort(by_key);
    expect_keys(r, { 1, 1, 2, 2, 3 });
    expect_ids(r, { 1, 3, 0, 2, 4 });

    // merge：稳定
    list_without_size<rec> m1{ { 1, 0 }, { 3, 1 } };
    list_without_size<rec> m2{ { 2, 2 }, { 3, 3 }, { 4, 4 } };
    m1.merge(m2, by_key);
    assert(m2.empty());
    expect_keys(m1, { 1, 2, 3, 3, 4 });
    expect_ids(m1, { 0, 2, 1, 3, 4 });

    // merge 的 rvalue 重载
    list_without_size<rec> m3{ { 0, 5 } };
    m1.merge(std::move(m3), by_key);
    assert(m3.empty());
    expect_keys(m1, { 0, 1, 2, 3, 3, 4 });

    // merge 自己 = 无效果
    m1.merge(m1, by_key);
    expect_keys(m1, { 0, 1, 2, 3, 3, 4 });

    // merge 默认 <
    list_without_size<int> i1{ 1, 3, 5 };
    list_without_size<int> i2{ 2, 4 };
    i1.merge(i2);
    assert(i2.empty());
    expect_vals(i1, { 1, 2, 3, 4, 5 });

    // sort 的比较器抛异常：元素一个都不能丢
    list_without_size<track> t;
    for (int v : { 5, 1, 4, 2, 3 })
    {
        t.emplace_back(v);
    }
    assert(track::alive == 5);
    int compares = 0;
    struct boom {};
    try
    {
        t.sort([&](const track& a, const track& b)
            {
                if (++compares == 3)
                {
                    throw boom{};
                }
                return a.v < b.v;
            });
        assert(false);
    }
    catch (const boom&)
    {
    }
    assert(track::alive == 5);              // 节点全部拼回来了
    assert(std::distance(t.begin(), t.end()) == 5);
    t.clear();
    assert(track::alive == 0);

    // reverse
    list_without_size<int> rv{ 1, 2, 3, 4 };
    auto keep = rv.begin();                 // 指向 1
    rv.reverse();
    expect_vals(rv, { 4, 3, 2, 1 });
    assert(*keep == 1);
    rv.reverse();
    expect_vals(rv, { 1, 2, 3, 4 });
    list_without_size<int> rv1{ 1 };
    rv1.reverse();
    expect_vals(rv1, { 1 });
    list_without_size<int> rv0;
    rv0.reverse();
    assert(rv0.empty());
    list_without_size<int> rv2{ 1, 2 };
    rv2.reverse();
    expect_vals(rv2, { 2, 1 });
}

// ---------------------------------------------------------------------------
// swap
// ---------------------------------------------------------------------------
static void test_swap()
{
    list_without_size<int> a{ 1, 2, 3 };
    list_without_size<int> b{ 4, 5 };
    auto ka = a.begin();
    a.swap(b);
    expect_vals(a, { 4, 5 });
    expect_vals(b, { 1, 2, 3 });
    assert(*ka == 1);                       // 节点跟着走，迭代器仍有效

    swap(a, b);
    expect_vals(a, { 1, 2, 3 });
    expect_vals(b, { 4, 5 });

    a.swap(a);
    expect_vals(a, { 1, 2, 3 });

    list_without_size<int> empty;
    a.swap(empty);
    assert(a.empty());
    expect_vals(empty, { 1, 2, 3 });

    using ia = list_without_size<int, id_allocator<int>>;
    ia x({ 1 }, id_allocator<int>{ 1 });
    ia y({ 2 }, id_allocator<int>{ 1 });    // 分配器相等才能 swap
    x.swap(y);
    expect_vals(x, { 2 });
    expect_vals(y, { 1 });
    assert(x.get_allocator().id == 1 && y.get_allocator().id == 1);

    // 分配次数必须配平
    alloc_stats::reset();
    {
        using ca = list_without_size<int, counting_allocator<int>>;
        ca p{ 1, 2 };
        ca q{ 3 };
        p.swap(q);
        expect_vals(p, { 3 });
        expect_vals(q, { 1, 2 });
    }
    assert(alloc_stats::balanced());
}

// ---------------------------------------------------------------------------
// 异常安全
// ---------------------------------------------------------------------------
static void test_exception_safety()
{
    // 拷贝构造中途抛异常：源链表不受影响，已插入的节点被回收
    alloc_stats::reset();
    throws_on_copy::alive = 0;
    throws_on_copy::countdown = -1;
    {
        using ca = list_without_size<throws_on_copy, counting_allocator<throws_on_copy>>;
        ca src;
        for (int i = 0; i != 6; ++i)
        {
            src.emplace_back(i);
        }
        assert(throws_on_copy::alive == 6);
        const std::size_t outstanding_before = alloc_stats::outstanding();

        throws_on_copy::countdown = 2;      // 第 3 次拷贝时抛
        bool threw = false;
        try
        {
            ca copy(src);
            (void)copy;
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        assert(threw);
        throws_on_copy::countdown = -1;

        // 源链表仍然完整，失败构造中分配的节点全部归还
        assert(throws_on_copy::alive == 6);
        assert(std::distance(src.begin(), src.end()) == 6);
        assert(alloc_stats::outstanding() == outstanding_before);

        // 区间 insert 中途抛异常：已插入的留下，原有元素不丢
        ca other(src);                      // 另一条链，作为插入源
        assert(throws_on_copy::alive == 12);
        const std::size_t outstanding_before2 = alloc_stats::outstanding();

        throws_on_copy::countdown = 2;
        threw = false;
        try
        {
            src.insert(src.begin(), other.begin(), other.end());
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        assert(threw);
        throws_on_copy::countdown = -1;
        assert(std::distance(src.begin(), src.end()) == 8);   // 6 + 2
        assert(throws_on_copy::alive == 14);                  // 6 + 6 + 2
        // 只多出两个已插入节点的分配，没有泄漏
        assert(alloc_stats::outstanding() == outstanding_before2 + 2);

        src.clear();
        other.clear();
        assert(throws_on_copy::alive == 0);
    }
    assert(alloc_stats::balanced());

    // 元素构造抛异常：不泄漏
    ctor_throws::alive = 0;
    alloc_stats::reset();
    {
        list_without_size<ctor_throws, counting_allocator<ctor_throws>> l;
        l.emplace_back(1);
        assert(ctor_throws::alive == 1);
        const std::size_t outstanding_before = alloc_stats::outstanding();
        bool threw = false;
        try
        {
            l.emplace_back(13);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        assert(threw);
        assert(ctor_throws::alive == 1);    // 只留下原来那个
        assert(alloc_stats::outstanding() == outstanding_before);   // 未构造成功的节点已归还
        l.clear();
        assert(ctor_throws::alive == 0);
    }
    assert(alloc_stats::balanced());
}

// ---------------------------------------------------------------------------
// 边界情况 / 单遍迭代器 / 大规模
// ---------------------------------------------------------------------------
static void test_edge_cases()
{
    // 空链表上 splice
    list_without_size<int> empty;
    list_without_size<int> full{ 1, 2 };
    empty.splice(empty.end(), full);
    expect_vals(empty, { 1, 2 });
    assert(full.empty());
    empty.splice(empty.begin(), full);          // other 为空：无效果
    expect_vals(empty, { 1, 2 });

    // merge 到空链表 / 用空链表 merge
    list_without_size<int> a{ 1, 3 };
    list_without_size<int> b;
    a.merge(b);                                  // 空源
    expect_vals(a, { 1, 3 });
    b.merge(a);                                  // 目标为空
    expect_vals(b, { 1, 3 });
    assert(a.empty());

    // pos == last 的区间 splice：无效果
    list_without_size<int> c{ 1, 2, 3, 4 };
    auto first = c.begin();
    auto last = first;
    ++last;
    ++last;                                      // [1,2)，pos == 3 == last
    c.splice(last, c, first, last);
    expect_vals(c, { 1, 2, 3, 4 });

    // insert 0 个元素：返回 pos
    auto it = c.insert(c.begin(), 0, 9);
    assert(it == c.begin());
    expect_vals(c, { 1, 2, 3, 4 });
    // insert 空区间：返回 pos
    it = c.insert(c.begin(), c.begin(), c.begin());
    assert(it == c.begin());

    // 单遍（input）迭代器
    std::istringstream iss("10 11 12");
    c.insert(c.end(), std::istream_iterator<int>(iss), std::istream_iterator<int>());
    expect_vals(c, { 1, 2, 3, 4, 10, 11, 12 });
    std::istringstream iss2("7 8");
    c.assign(std::istream_iterator<int>(iss2), std::istream_iterator<int>());
    expect_vals(c, { 7, 8 });

    // 构造 / 赋值用 input iterator
    std::istringstream iss3("1 2 3");
    list_without_size<int> from_input((std::istream_iterator<int>(iss3)), (std::istream_iterator<int>()));
    expect_vals(from_input, { 1, 2, 3 });

    // 大规模：排序 / 逆序 / 删除
    list_without_size<int> big;
    for (int i = 0; i != 1000; ++i)
    {
        big.push_back((i * 37) % 1000);
    }
    big.sort();
    assert(std::distance(big.begin(), big.end()) == 1000);
    int prev = -1;
    for (int v : big)
    {
        assert(prev <= v);
        prev = v;
    }
    big.reverse();
    int prev2 = 1000;
    for (int v : big)
    {
        assert(v <= prev2);
        prev2 = v;
    }
    assert(big.remove_if([](int v) { return v % 2 == 0; }) == 500);
    assert(std::distance(big.begin(), big.end()) == 500);
    for (int v : big)
    {
        assert(v % 2 != 0);
    }

    // 全等 / 全删
    list_without_size<int> all{ 5, 5, 5, 5 };
    assert(all.unique() == 3);
    expect_vals(all, { 5 });
    assert(all.remove(5) == 1);
    assert(all.empty());

    // erase 循环清空
    list_without_size<int> e{ 1, 2, 3 };
    for (auto i = e.begin(); i != e.end();)
    {
        i = e.erase(i);
    }
    assert(e.empty());

    // 自 move 赋值
    list_without_size<int> self{ 1, 2 };
    auto& ref = self;
    self = std::move(ref);
    expect_vals(self, { 1, 2 });

    // 自拷贝赋值
    self = static_cast<const list_without_size<int>&>(self);
    expect_vals(self, { 1, 2 });

    // 空链表的拷贝/移动
    list_without_size<int> e0;
    list_without_size<int> e1(e0);
    list_without_size<int> e2(std::move(e0));
    assert(e1.empty() && e2.empty());
    e1 = e2;
    assert(e1.empty());
}

// ---------------------------------------------------------------------------
// 只能移动的元素类型
// ---------------------------------------------------------------------------
static void test_move_only()
{
    list_without_size<std::unique_ptr<int>> l;
    l.push_back(std::make_unique<int>(1));
    l.push_back(std::make_unique<int>(2));
    l.emplace_front(std::make_unique<int>(0));
    assert(std::distance(l.begin(), l.end()) == 3);
    assert(**l.begin() == 0);
    assert(**l.rbegin() == 2);

    auto taken = std::move(l.front());
    assert(*taken == 0);
    l.pop_front();
    assert(std::distance(l.begin(), l.end()) == 2);

    list_without_size<std::unique_ptr<int>> m;
    m.splice(m.end(), l);
    assert(l.empty());
    assert(**m.begin() == 1);

    m.reverse();
    assert(**m.begin() == 2);
    m.clear();
    assert(m.empty());

    // sort 只搬节点、不搬元素：move-only 类型也能排序
    list_without_size<std::unique_ptr<int>> sv;
    sv.push_back(std::make_unique<int>(3));
    sv.push_back(std::make_unique<int>(1));
    sv.push_back(std::make_unique<int>(2));
    sv.sort([](const std::unique_ptr<int>& a, const std::unique_ptr<int>& b) { return *a < *b; });
    assert(**sv.begin() == 1);
    assert(**sv.rbegin() == 3);

    // 移动赋值 + 逐元素移动路径
    list_without_size<std::unique_ptr<int>> n;
    n.push_back(std::make_unique<int>(5));
    n = std::move(m);
    assert(n.empty());
}

// ---------------------------------------------------------------------------
// 与 std::list 的差分测试：同一串随机操作，两边结果必须完全一致
// ---------------------------------------------------------------------------
static void test_against_std_list()
{
    std::mt19937 rng(20231109u);
    list_without_size<int> yc;
    std::list<int> sl;

    auto same = [&]
    {
        assert(yc.empty() == sl.empty());
        std::vector<int> a(vals(yc));
        std::vector<int> b(sl.begin(), sl.end());
        assert(a == b);
    };
    auto pick = [&](std::size_t n)
    {
        return n == 0 ? std::size_t{ 0 } : std::size_t{ rng() % n };
    };
    auto advance_to = [](auto first, std::size_t n)
    {
        std::advance(first, (std::ptrdiff_t)n);
        return first;
    };

    for (int step = 0; step != 2000; ++step)
    {
        const std::size_t n = (std::size_t)std::distance(sl.begin(), sl.end());
        const int v = (int)(rng() % 20);
        switch ((int)(rng() % 14))
        {
        case 0:
            yc.push_back(v);
            sl.push_back(v);
            break;
        case 1:
            yc.push_front(v);
            sl.push_front(v);
            break;
        case 2:
            if (n != 0)
            {
                yc.pop_back();
                sl.pop_back();
            }
            break;
        case 3:
            if (n != 0)
            {
                yc.pop_front();
                sl.pop_front();
            }
            break;
        case 4:
        {
            const std::size_t p = pick(n);
            yc.insert(advance_to(yc.begin(), p), v);
            sl.insert(advance_to(sl.begin(), p), v);
            break;
        }
        case 5:
        {
            if (n != 0)
            {
                const std::size_t p = pick(n);
                yc.erase(advance_to(yc.begin(), p));
                sl.erase(advance_to(sl.begin(), p));
            }
            break;
        }
        case 6:
            assert(yc.remove(v) == sl.remove(v));
            break;
        case 7:
        {
            auto even = [](int x) { return x % 2 == 0; };
            assert(yc.remove_if(even) == sl.remove_if(even));
            break;
        }
        case 8:
            yc.sort();
            sl.sort();
            break;
        case 9:
            yc.reverse();
            sl.reverse();
            break;
        case 10:
            assert(yc.unique() == sl.unique());
            break;
        case 11:
            yc.resize((std::size_t)(v % 10));
            sl.resize((std::size_t)(v % 10));
            break;
        case 12:
        {
            // 从临时链表 splice 一段到随机位置
            list_without_size<int> t1;
            std::list<int> t2;
            for (int i = 0, m = v % 4; i != m; ++i)
            {
                t1.push_back(v + i);
                t2.push_back(v + i);
            }
            const std::size_t p = pick(n);
            yc.splice(advance_to(yc.begin(), p), t1);
            sl.splice(advance_to(sl.begin(), p), t2);
            assert(t1.empty() && t2.empty());
            break;
        }
        case 13:
        {
            // 两边先排序，再与有序临时链表 merge
            list_without_size<int> t1;
            std::list<int> t2;
            int cur = v % 5;
            for (int i = 0; i != 3; ++i)
            {
                cur += 1 + (int)(rng() % 3);
                t1.push_back(cur);
                t2.push_back(cur);
            }
            yc.sort();
            sl.sort();
            yc.merge(t1);
            sl.merge(t2);
            assert(t1.empty() && t2.empty());
            break;
        }
        default:
            break;
        }
        same();
    }

    // 收尾：逐个清空，比较删除个数
    while (!sl.empty())
    {
        assert(yc.back() == sl.back());
        yc.pop_back();
        sl.pop_back();
    }
    same();
}

// ---------------------------------------------------------------------------
// 缀饰指针：vptr 取自 Alloc::void_pointer，整条链都应改用 fancy 指针
// ---------------------------------------------------------------------------
static void test_fancy_pointer()
{
    using fa = list_without_size<int, fancy_allocator<int>>;
    static_assert(std::is_same_v<fa::pointer, fancy_ptr<int>>);
    static_assert(std::is_same_v<fa::const_pointer, fancy_ptr<const int>>);
    static_assert(!std::is_pointer_v<fa::pointer>);
    // 迭代器内部保存的确实是缀饰指针，而不是偷偷退化成了原始指针
    static_assert(!std::is_pointer_v<decltype(std::declval<fa::iterator>().node())>);
    static_assert(!std::is_pointer_v<decltype(std::declval<fa::const_iterator>().node())>);
    static_assert(std::bidirectional_iterator<fa::iterator>);

    fa l{ 3, 1, 2 };
    assert(std::distance(l.begin(), l.end()) == 3);
    assert(*l.begin() == 3);
    assert(l.front() == 3);
    assert(l.back() == 2);

    l.push_back(4);
    l.push_front(0);
    expect_vals(l, { 0, 3, 1, 2, 4 });
    l.sort();
    expect_vals(l, { 0, 1, 2, 3, 4 });
    l.reverse();
    expect_vals(l, { 4, 3, 2, 1, 0 });

    // 反向迭代器
    assert(*l.rbegin() == 0);
    std::vector<int> rv(l.rbegin(), l.rend());
    assert((rv == std::vector<int>{ 0, 1, 2, 3, 4 }));

    // erase / insert / emplace
    auto it = l.begin();
    ++it;
    l.erase(it);
    expect_vals(l, { 4, 2, 1, 0 });
    it = l.insert(l.begin(), 9);
    assert(*it == 9);
    it = l.emplace(l.end(), 8);
    assert(*it == 8);
    expect_vals(l, { 9, 4, 2, 1, 0, 8 });

    // splice / merge
    fa m{ 7, 6 };
    m.sort();
    l.sort();
    l.merge(m);
    assert(m.empty());
    expect_vals(l, { 0, 1, 2, 4, 6, 7, 8, 9 });

    // const 迭代器（缀饰的 const 指针）与跨 const 比较
    const fa& cl = l;
    static_assert(std::is_same_v<decltype(cl.begin()), fa::const_iterator>);
    assert(std::distance(cl.begin(), cl.end()) == 8);
    assert(*cl.begin() == 0);
    fa::const_iterator ci = l.begin();
    assert(ci == l.begin());
    assert(l.begin() == ci);
    assert(ci == cl.begin());

    // 拷贝 / 移动 / 赋值
    fa cp(l);
    expect_vals(cp, { 0, 1, 2, 4, 6, 7, 8, 9 });
    fa mv(std::move(cp));
    assert(cp.empty());
    expect_vals(mv, { 0, 1, 2, 4, 6, 7, 8, 9 });
    cp = mv;
    expect_vals(cp, { 0, 1, 2, 4, 6, 7, 8, 9 });
    cp = std::move(mv);
    assert(mv.empty());
    expect_vals(cp, { 0, 1, 2, 4, 6, 7, 8, 9 });

    // remove / remove_if / unique
    cp.push_back(9);
    assert(cp.remove(9) == 2);
    assert(cp.unique() == 0);
    assert(cp.remove_if([](int v) { return v % 2 == 0; }) == 5);
    expect_vals(cp, { 1, 7 });

    // 元素构造/析构也走缀饰分配器
    using ft = list_without_size<track, fancy_allocator<track>>;
    {
        ft t;
        t.emplace_back(1);
        t.emplace_back(2);
        assert(track::alive == 2);
        t.pop_front();
        assert(track::alive == 1);
        t.resize(3, track(7));
        assert(track::alive == 3);
        t.clear();
        assert(track::alive == 0);
    }
    assert(track::alive == 0);

    // 与标准算法互操作
    int sum = 0;
    for (int v : l)
    {
        sum += v;
    }
    assert(sum == 0 + 1 + 2 + 4 + 6 + 7 + 8 + 9);
}

// ---------------------------------------------------------------------------
// 偏移量缀饰指针：内部不保存绝对地址，任何"藏起来的原始指针"都会暴露
// ---------------------------------------------------------------------------
static void test_offset_pointer()
{
    using oa = list_without_size<int, offset_allocator<int>>;
    static_assert(std::is_same_v<oa::pointer, offset_ptr<int>>);
    static_assert(!std::is_pointer_v<decltype(std::declval<oa::iterator>().node())>);

    oa l{ 5, 1, 4, 2, 3 };
    assert(std::distance(l.begin(), l.end()) == 5);
    assert(l.front() == 5);
    assert(l.back() == 3);
    l.sort();
    expect_vals(l, { 1, 2, 3, 4, 5 });
    l.reverse();
    expect_vals(l, { 5, 4, 3, 2, 1 });
    l.reverse();
    expect_vals(l, { 1, 2, 3, 4, 5 });

    auto it = l.begin();
    ++it;
    it = l.erase(it);
    assert(*it == 3);
    expect_vals(l, { 1, 3, 4, 5 });

    it = l.insert(l.begin(), 0);
    assert(*it == 0);
    expect_vals(l, { 0, 1, 3, 4, 5 });

    oa m{ 6, 7 };
    l.splice(l.end(), m);
    assert(m.empty());
    expect_vals(l, { 0, 1, 3, 4, 5, 6, 7 });

    l.sort();
    oa n{ 2, 8 };
    n.sort();
    l.merge(n);
    assert(n.empty());
    expect_vals(l, { 0, 1, 2, 3, 4, 5, 6, 7, 8 });

    oa cp(l);
    oa mv(std::move(cp));
    assert(cp.empty());
    expect_vals(mv, { 0, 1, 2, 3, 4, 5, 6, 7, 8 });
    cp = mv;
    expect_vals(cp, { 0, 1, 2, 3, 4, 5, 6, 7, 8 });

    // 反向迭代 / const 迭代
    const oa& cl = cp;
    assert(std::distance(cl.begin(), cl.end()) == 9);
    assert(*cl.rbegin() == 8);
    assert(*cl.crbegin() == 8);
    oa::const_iterator ci = cp.begin();
    assert(ci == cp.begin());
    assert(cp.begin() == ci);

    // 插入/删除其他元素后，已有迭代器仍指向原元素
    auto keep = cp.begin();
    ++keep;
    cp.push_front(-1);
    assert(*keep == 1);

    cp.clear();
    assert(cp.empty());

    // 元素构造/析构也走偏移量分配器
    using ot = list_without_size<track, offset_allocator<track>>;
    {
        ot t;
        t.emplace_back(1);
        t.emplace_back(2);
        t.emplace_back(3);
        assert(track::alive == 3);
        t.pop_back();
        assert(track::alive == 2);
        t.resize(1);
        assert(track::alive == 1);
    }
    assert(track::alive == 0);
}

// ---------------------------------------------------------------------------
// 分配器传播 / 有状态分配器
// ---------------------------------------------------------------------------
static void test_allocator_behavior()
{
    using ia = list_without_size<int, id_allocator<int>>;

    // 拷贝赋值：分配器不传播（id_allocator 默认 POCMA/POCCA/POCS 均 false）
    ia a{ 1, 2 };
    ia b({ 3 }, id_allocator<int>{ 5 });
    a = b;
    expect_vals(a, { 3 });
    assert(a.get_allocator().id == 0);       // 分配器没被换掉
    assert(b.get_allocator().id == 5);

    // 移动赋值：分配器不相等且不传播 → 逐元素移动
    ia c(id_allocator<int>{ 2 });
    c = std::move(b);
    expect_vals(c, { 3 });
    assert(c.get_allocator().id == 2);       // 分配器不变
    assert(b.empty());

    // select_on_container_copy_construction 默认返回同一分配器
    ia d(a);
    assert(d.get_allocator().id == a.get_allocator().id);
    expect_vals(d, { 3 });

    // max_size 来自节点分配器
    assert(a.max_size() > 0);

    // 分配器会传播的情况：POCCA / POCMA / POCS 全为 true
    using pa = list_without_size<int, prop_allocator<int>>;
    pa p1({ 1, 2 }, prop_allocator<int>{ 1 });
    pa p2({ 3 }, prop_allocator<int>{ 2 });
    p1 = p2;                                 // 拷贝赋值传播分配器
    expect_vals(p1, { 3 });
    assert(p1.get_allocator().id == 2);
    assert(p2.get_allocator().id == 2);

    pa p3({ 5 }, prop_allocator<int>{ 3 });
    p3 = std::move(p2);                      // 移动赋值传播分配器并接管节点
    expect_vals(p3, { 3 });
    assert(p3.get_allocator().id == 2);
    assert(p2.empty());

    pa p4({ 7 }, prop_allocator<int>{ 4 });
    p3.swap(p4);                             // swap 传播分配器
    expect_vals(p3, { 7 });
    expect_vals(p4, { 3 });
    assert(p3.get_allocator().id == 4);
    assert(p4.get_allocator().id == 2);

    // 带分配器的拷贝构造用的是显式传入的分配器
    pa p5(p3, prop_allocator<int>{ 6 });
    assert(p5.get_allocator().id == 6);
    expect_vals(p5, { 7 });
}

int main()
{
    test_basic();
    test_construct_assign();
    test_modifiers();
    test_splice();
    test_remove_unique();
    test_sort_merge_reverse();
    test_swap();
    test_exception_safety();
    test_edge_cases();
    test_move_only();
    test_against_std_list();
    test_fancy_pointer();
    test_offset_pointer();
    test_allocator_behavior();

    assert(track::alive == 0);
    assert(throws_on_copy::alive == 0);
    std::puts("test_list_without_size: ok");
}
