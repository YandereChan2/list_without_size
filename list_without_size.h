#pragma once
// ===========================================================================
// Yc::list_without_size<T, Alloc>
//
// 用 intrusive_list.h 里的设施实现的 std::list：成员函数与 std::list 保持一致，
// 唯一不提供的是 size()。链表本身不记录元素个数，因此没有 O(1) 的 size 可返回
// （确需个数时用 std::distance(l.begin(), l.end())，代价 O(n)）。
// 省掉的"计数"换来的是：插入/删除/splice 都只改指针，不需要维护计数器；
// 对象里只留下一个哨兵（两个指针），空分配器还会被 EBO 压缩掉。
// 覆盖 C++23 的 std::list：含 insert_range / append_range / prepend_range /
// assign_range 以及 from_range 构造。
//
// 分工：
//   * intrusive_list_node<void_pointer, T> 就地存放 T（构造/析构仍走分配器）；
//   * basic_intrusive_list<void_pointer> 负责哨兵、插入、摘除、splice、稳定排序、
//     稳定合并、逆序等全部链表指针操作；
//   * 本类只做三件事：分配/释放节点、把"节点迭代器"适配成"元素迭代器"、
//     把作用于节点的谓词/比较器适配成作用于 T 的谓词/比较器。
//
// 节点指针类型 void_pointer 取自分配器（allocator_traits<Alloc>::void_pointer），
// 所以分配器支持缀饰指针（fancy pointer）时整条链也都是缀饰指针：普通分配器
// 得到 void*，fancy 分配器得到对应的 fancy<void>，节点、哨兵、迭代器全部统一。
//
// 与 std::list 相同：元素就地存放，插入/删除/splice 不会使其余元素的迭代器
// 或引用失效，也不会搬移元素；splice/merge 只搬节点，O(1)/O(n) 且不分配。
// ===========================================================================
#include "intrusive_list.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace Yc
{
    template<class T, class Alloc>
    class list_without_size;

    namespace details
    {
        // 把 basic_intrusive_list 的"节点迭代器"包装成 std::list 风格的
        // "元素迭代器"：解引用得到 T&（const 版本得到 const T&），其余双向
        // 迭代器行为（++/--/后置/比较）原样转发给底层迭代器。
        // vptr 是侵入式链表使用的"void 指针"类型：可以是 void*，也可以是
        // 分配器提供的缀饰指针。迭代器只保存节点指针，节点在链表里就地存放，
        // 所以只要节点还在链表里，迭代器就一直有效。
        template<class T, class vptr, bool Const>
        class list_without_size_iterator
        {
        public:
            using intrusive_node = intrusive_list_node<vptr, T>;
            using node_base = intrusive_list_node_base<vptr>;
            using base_iterator = std::conditional_t<Const,
                basic_intrusive_list_const_iterator<vptr>,
                basic_intrusive_list_iterator<vptr>>;
        private:
            base_iterator cur{};
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using iterator_concept = std::bidirectional_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = std::conditional_t<Const, const T*, T*>;
            using reference = std::conditional_t<Const, const T&, T&>;

            list_without_size_iterator() = default;
            explicit list_without_size_iterator(base_iterator it)noexcept : cur(it) {}
            // 允许 iterator -> const_iterator（单向，和 std::list 一致）
            template<bool OtherConst, std::enable_if_t<Const && !OtherConst, int> = 0>
            list_without_size_iterator(const list_without_size_iterator<T, vptr, OtherConst>& other)noexcept
                : cur(base_iterator{ other.node() }) {}

            reference operator*()const noexcept
            {
                if constexpr (Const)
                {
                    return static_cast<const intrusive_node&>(*cur).value();
                }
                else
                {
                    return static_cast<intrusive_node&>(*cur).value();
                }
            }
            pointer operator->()const noexcept
            {
                return std::addressof(operator*());
            }
            list_without_size_iterator& operator++()noexcept
            {
                ++cur;
                return *this;
            }
            list_without_size_iterator& operator--()noexcept
            {
                --cur;
                return *this;
            }
            list_without_size_iterator operator++(int)noexcept
            {
                auto tmp = *this;
                ++*this;
                return tmp;
            }
            list_without_size_iterator operator--(int)noexcept
            {
                auto tmp = *this;
                --*this;
                return tmp;
            }
            // 底层节点指针（可能是缀饰指针）：链表内部靠它定位节点或哨兵
            auto node()const noexcept
            {
                return cur.base();
            }
            // 同一类型的迭代器直接比较；iterator 与 const_iterator 之间的比较
            // 靠 iterator -> const_iterator 的隐式转换完成（与 std::list 一致）。
            friend bool operator==(const list_without_size_iterator& lhs,
                const list_without_size_iterator& rhs)noexcept
            {
                return lhs.cur == rhs.cur;
            }
        };

        // 空分配器用"继承"做 EBO：[[no_unique_address]] 在 MSVC 上并不生效
        // （实测带该属性的空成员仍占一个指针的位置），继承则一定能压缩掉。
        // 有状态/非空分配器退化成普通成员，没有额外开销。
        // 空且 final 的分配器不能作为基类，也走普通成员版本。
        template<class Alloc, bool Empty = std::is_empty_v<Alloc> && !std::is_final_v<Alloc>>
        class alloc_holder
        {
            Alloc alloc;
        public:
            alloc_holder() = default;
            explicit alloc_holder(const Alloc& a) : alloc(a) {}
            explicit alloc_holder(Alloc&& a) : alloc(std::move(a)) {}
            Alloc& get()noexcept
            {
                return alloc;
            }
            const Alloc& get()const noexcept
            {
                return alloc;
            }
        };

        template<class Alloc>
        class alloc_holder<Alloc, true> : private Alloc
        {
        public:
            alloc_holder() = default;
            explicit alloc_holder(const Alloc& a) : Alloc(a) {}
            explicit alloc_holder(Alloc&& a) : Alloc(std::move(a)) {}
            Alloc& get()noexcept
            {
                return *this;
            }
            const Alloc& get()const noexcept
            {
                return *this;
            }
        };
    }

    template<class T, class Alloc = std::allocator<T>>
    class list_without_size : private details::alloc_holder<Alloc>
    {
    private:
        using alloc_traits = std::allocator_traits<Alloc>;
        // 侵入式链表的节点指针类型：直接用分配器给的 void 指针，
        // 普通分配器是 void*，缀饰分配器是对应的 fancy<void>。
        using void_pointer = typename alloc_traits::void_pointer;

    public:
        using value_type = T;
        using allocator_type = Alloc;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename alloc_traits::pointer;
        using const_pointer = typename alloc_traits::const_pointer;
        using iterator = details::list_without_size_iterator<T, void_pointer, false>;
        using const_iterator = details::list_without_size_iterator<T, void_pointer, true>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    private:
        using node_base = intrusive_list_node_base<void_pointer>;
        using node_type = intrusive_list_node<void_pointer, T>;
        using list_type = basic_intrusive_list<void_pointer>;
        using base_iterator = typename list_type::iterator;
        using node_pointer = typename std::pointer_traits<void_pointer>::template rebind<node_type>;
        using node_base_pointer = typename std::pointer_traits<void_pointer>::template rebind<node_base>;
        using alloc_holder_type = details::alloc_holder<Alloc>;
        using node_allocator = typename alloc_traits::template rebind_alloc<node_type>;
        using node_alloc_traits = std::allocator_traits<node_allocator>;

        // 分配器给节点的指针必须就是侵入式链表按 void_pointer 推导出的节点指针，
        // 否则节点挂不进 basic_intrusive_list<void_pointer>。
        static_assert(std::is_same_v<node_pointer, typename node_alloc_traits::pointer>,
            "list_without_size 要求分配器对节点的 pointer 等于 "
            "pointer_traits<Alloc::void_pointer>::rebind<节点类型>（侵入式链表用 Alloc::void_pointer 作节点指针）");

        Alloc& alloc_ref()noexcept
        {
            return alloc_holder_type::get();
        }
        const Alloc& alloc_ref()const noexcept
        {
            return alloc_holder_type::get();
        }

        // ---- 节点 <-> 元素 ----
        static reference value_of(node_base& n)noexcept
        {
            return static_cast<node_type&>(n).value();
        }
        static const_reference value_of(const node_base& n)noexcept
        {
            return static_cast<const node_type&>(n).value();
        }
        // 从 const 迭代器取回"节点基类"指针。缀饰指针不能 const_cast，
        // 于是按 pointer_traits 的通用做法绕一圈：取引用 -> 去掉 const -> pointer_to。
        static node_base_pointer node_of(const_iterator pos)noexcept
        {
            return std::pointer_traits<node_base_pointer>::pointer_to(const_cast<node_base&>(*pos.node()));
        }
        // "节点基类"引用/指针 -> 具体节点指针（那层派生只有本类知道）。
        // 同样不能 static_cast 缀饰指针，所以经由引用 + pointer_to。
        static node_pointer as_node(node_base& n)noexcept
        {
            return std::pointer_traits<node_pointer>::pointer_to(static_cast<node_type&>(n));
        }
        static node_pointer as_node(const node_base_pointer& p)noexcept
        {
            return as_node(*p);
        }
        static base_iterator base_of(const_iterator pos)noexcept
        {
            return base_iterator{ node_of(pos) };
        }
        static iterator to_iterator(const_iterator pos)noexcept
        {
            return iterator{ base_of(pos) };
        }

        // ---- 节点生命周期 ----
        // 分配并构造一个节点：先构造节点骨架（prev/next 自指），再就地构造 T。
        // 任一步抛异常都把已完成的步骤倒掉，不会留下半成品。
        // 节点指针可能是缀饰指针，所以"要原始地址"的地方统一用 std::to_address。
        template<class... Args>
        node_pointer create_node(Args&&... args)
        {
            node_allocator na{ alloc_ref() };
            node_pointer p = node_alloc_traits::allocate(na, 1);
            node_type* raw = std::to_address(p);
            try
            {
                node_alloc_traits::construct(na, raw);
            }
            catch (...)
            {
                node_alloc_traits::deallocate(na, p, 1);
                throw;
            }
            try
            {
                raw->construct_value(alloc_ref(), std::forward<Args>(args)...);
            }
            catch (...)
            {
                node_alloc_traits::destroy(na, raw);
                node_alloc_traits::deallocate(na, p, 1);
                throw;
            }
            return p;
        }
        void destroy_node(node_pointer p)noexcept
        {
            node_allocator na{ alloc_ref() };
            node_type* raw = std::to_address(p);
            raw->destroy_value(alloc_ref());
            node_alloc_traits::destroy(na, raw);
            node_alloc_traits::deallocate(na, p, 1);
        }

        // 在 pos 之前挂接一个新建节点；pos 是 end() 时即"挂到尾部"。
        template<class... Args>
        iterator emplace_before(base_iterator pos, Args&&... args)
        {
            node_pointer p = create_node(std::forward<Args>(args)...);
            return iterator{ list_type::insert(pos, *p) };
        }

        // std::list 的 splice/merge/swap 要求两个链表的分配器相等；
        // 不相等时标准规定行为未定义，这里在 Debug 下抓出来。
        void assert_compatible(const list_without_size& other)const noexcept
        {
            (void)other;
            if constexpr (!alloc_traits::is_always_equal::value)
            {
                assert(alloc_ref() == other.alloc_ref());
            }
        }

        // 移动构造/移动赋值里"另一个链表还能不能用我的分配器接管"的分支：
        // 能就整体 splice（O(1)、不搬元素），不能就逐元素移动。
        void take_from(list_without_size& other)
        {
            if constexpr (alloc_traits::is_always_equal::value)
            {
                impl_.splice(impl_.end(), other.impl_);
            }
            else if (alloc_ref() == other.alloc_ref())
            {
                impl_.splice(impl_.end(), other.impl_);
            }
            else
            {
                auto it = other.begin();
                while (it != other.end())
                {
                    emplace_back(std::move(*it));
                    it = other.erase(it);
                }
            }
        }

    public:
        // ===================== 构造 / 析构 =====================
        list_without_size() = default;
        explicit list_without_size(const Alloc& alloc) : alloc_holder_type(alloc) {}
        explicit list_without_size(size_type n, const Alloc& alloc = Alloc()) : alloc_holder_type(alloc)
        {
            try
            {
                for (size_type i = 0; i != n; ++i)
                {
                    emplace_back();
                }
            }
            catch (...)
            {
                clear();        // 构造期间抛异常不会走析构函数，节点得自己回收
                throw;
            }
        }
        list_without_size(size_type n, const T& value, const Alloc& alloc = Alloc()) : alloc_holder_type(alloc)
        {
            try
            {
                insert(cend(), n, value);
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        template<std::input_iterator InputIt>
        list_without_size(InputIt first, InputIt last, const Alloc& alloc = Alloc()) : alloc_holder_type(alloc)
        {
            try
            {
                insert(cend(), first, last);
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        list_without_size(const list_without_size& other)
            : alloc_holder_type(alloc_traits::select_on_container_copy_construction(other.alloc_ref()))
        {
            try
            {
                insert(cend(), other.begin(), other.end());
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        list_without_size(list_without_size&& other)noexcept(alloc_traits::is_always_equal::value)
            : alloc_holder_type(std::move(static_cast<alloc_holder_type&>(other)))
        {
            impl_.splice(impl_.end(), other.impl_);
        }
        list_without_size(const list_without_size& other, const Alloc& alloc) : alloc_holder_type(alloc)
        {
            try
            {
                insert(cend(), other.begin(), other.end());
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        list_without_size(list_without_size&& other, const Alloc& alloc) : alloc_holder_type(alloc)
        {
            try
            {
                take_from(other);
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        list_without_size(std::initializer_list<T> il, const Alloc& alloc = Alloc()) : alloc_holder_type(alloc)
        {
            try
            {
                insert(cend(), il.begin(), il.end());
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        template<std::ranges::input_range R>
        list_without_size(std::from_range_t, R&& rg, const Alloc& alloc = Alloc()) : alloc_holder_type(alloc)
        {
            try
            {
                insert(cend(), std::ranges::begin(rg), std::ranges::end(rg));
            }
            catch (...)
            {
                clear();
                throw;
            }
        }
        ~list_without_size()
        {
            clear();
        }

        // ===================== 赋值 =====================
        list_without_size& operator=(const list_without_size& other)
        {
            if (this == &other)
            {
                return *this;
            }
            clear();        // 先用旧分配器把节点还掉，再换分配器/复制元素
            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value)
            {
                alloc_ref() = other.alloc_ref();
            }
            insert(cend(), other.begin(), other.end());
            return *this;
        }
        list_without_size& operator=(list_without_size&& other)noexcept(
            alloc_traits::propagate_on_container_move_assignment::value
            || alloc_traits::is_always_equal::value)
        {
            if (this == &other)
            {
                return *this;
            }
            clear();
            if constexpr (alloc_traits::propagate_on_container_move_assignment::value)
            {
                // 分配器跟着一起搬，节点可以直接接管
                alloc_ref() = std::move(other.alloc_ref());
                impl_.splice(impl_.end(), other.impl_);
            }
            else
            {
                take_from(other);
            }
            return *this;
        }
        list_without_size& operator=(std::initializer_list<T> il)
        {
            assign(il.begin(), il.end());
            return *this;
        }
        allocator_type get_allocator()const noexcept
        {
            return alloc_ref();
        }

        // ===================== 迭代器 =====================
        iterator begin()noexcept
        {
            return iterator{ impl_.begin() };
        }
        iterator end()noexcept
        {
            return iterator{ impl_.end() };
        }
        const_iterator begin()const noexcept
        {
            return const_iterator{ impl_.begin() };
        }
        const_iterator end()const noexcept
        {
            return const_iterator{ impl_.end() };
        }
        const_iterator cbegin()const noexcept
        {
            return begin();
        }
        const_iterator cend()const noexcept
        {
            return end();
        }
        reverse_iterator rbegin()noexcept
        {
            return reverse_iterator{ end() };
        }
        reverse_iterator rend()noexcept
        {
            return reverse_iterator{ begin() };
        }
        const_reverse_iterator rbegin()const noexcept
        {
            return const_reverse_iterator{ end() };
        }
        const_reverse_iterator rend()const noexcept
        {
            return const_reverse_iterator{ begin() };
        }
        const_reverse_iterator crbegin()const noexcept
        {
            return rbegin();
        }
        const_reverse_iterator crend()const noexcept
        {
            return rend();
        }

        // ===================== 容量 =====================
        // 没有 size()：链表不记录元素个数。empty() 仍然是 O(1)。
        bool empty()const noexcept
        {
            return impl_.empty();
        }
        size_type max_size()const noexcept
        {
            return node_alloc_traits::max_size(node_allocator{ alloc_ref() });
        }

        // ===================== 元素访问 =====================
        reference front()noexcept
        {
            return *begin();
        }
        const_reference front()const noexcept
        {
            return *begin();
        }
        reference back()noexcept
        {
            auto it = end();
            --it;
            return *it;
        }
        const_reference back()const noexcept
        {
            auto it = end();
            --it;
            return *it;
        }

        // ===================== 修改 =====================
        void clear()noexcept
        {
            auto it = impl_.begin();
            while (it != impl_.end())
            {
                node_base_pointer p = it.base();
                it = impl_.erase(it);       // 先摘链（拿到下一个位置）再销毁节点
                destroy_node(as_node(p));
            }
        }

        // assign 系列：先覆写已有元素，再补插/截断多出来的部分。
        // 这样 value 引用链内元素时也安全（不会先 clear() 把 value 销毁）。
        void assign(size_type n, const T& value)
        {
            auto it = begin();
            size_type i = 0;
            for (; i != n && it != end(); ++i, ++it)
            {
                *it = value;
            }
            if (i == n)
            {
                erase(it, end());
            }
            else
            {
                insert(end(), n - i, value);
            }
        }
        template<std::input_iterator InputIt>
        void assign(InputIt first, InputIt last)
        {
            auto it = begin();
            for (; first != last && it != end(); ++first, ++it)
            {
                *it = *first;
            }
            if (first == last)
            {
                erase(it, end());
            }
            else
            {
                insert(end(), first, last);
            }
        }
        void assign(std::initializer_list<T> il)
        {
            assign(il.begin(), il.end());
        }
        template<std::ranges::input_range R>
        void assign_range(R&& rg)
        {
            assign(std::ranges::begin(rg), std::ranges::end(rg));
        }

        template<class... Args>
        iterator emplace(const_iterator pos, Args&&... args)
        {
            return emplace_before(base_of(pos), std::forward<Args>(args)...);
        }
        iterator insert(const_iterator pos, const T& value)
        {
            return emplace(pos, value);
        }
        iterator insert(const_iterator pos, T&& value)
        {
            return emplace(pos, std::move(value));
        }
        iterator insert(const_iterator pos, size_type n, const T& value)
        {
            base_iterator where = base_of(pos);
            iterator result = to_iterator(pos);
            bool first_one = true;
            for (size_type i = 0; i != n; ++i)
            {
                auto it = emplace_before(where, value);
                if (first_one)
                {
                    result = it;
                    first_one = false;
                }
            }
            return result;
        }
        template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            base_iterator where = base_of(pos);
            iterator result = to_iterator(pos);
            bool first_one = true;
            for (; first != last; ++first)
            {
                auto it = emplace_before(where, *first);
                if (first_one)
                {
                    result = it;
                    first_one = false;
                }
            }
            return result;
        }
        iterator insert(const_iterator pos, std::initializer_list<T> il)
        {
            return insert(pos, il.begin(), il.end());
        }
        template<std::ranges::input_range R>
        iterator insert_range(const_iterator pos, R&& rg)
        {
            return insert(pos, std::ranges::begin(rg), std::ranges::end(rg));
        }
        template<std::ranges::input_range R>
        void append_range(R&& rg)
        {
            insert(cend(), std::ranges::begin(rg), std::ranges::end(rg));
        }
        template<std::ranges::input_range R>
        void prepend_range(R&& rg)
        {
            insert(cbegin(), std::ranges::begin(rg), std::ranges::end(rg));
        }

        iterator erase(const_iterator pos)noexcept
        {
            node_base_pointer p = node_of(pos);
            auto next = impl_.erase(base_iterator{ p });
            destroy_node(as_node(p));
            return iterator{ next };
        }
        iterator erase(const_iterator first, const_iterator last)noexcept
        {
            while (first != last)
            {
                first = erase(first);
            }
            return to_iterator(last);
        }

        void push_back(const T& value)
        {
            emplace_back(value);
        }
        void push_back(T&& value)
        {
            emplace_back(std::move(value));
        }
        template<class... Args>
        reference emplace_back(Args&&... args)
        {
            node_pointer p = create_node(std::forward<Args>(args)...);
            impl_.push_back(*p);
            return std::to_address(p)->value();
        }
        void pop_back()noexcept
        {
            auto it = end();
            --it;
            erase(it);
        }
        void push_front(const T& value)
        {
            emplace_front(value);
        }
        void push_front(T&& value)
        {
            emplace_front(std::move(value));
        }
        template<class... Args>
        reference emplace_front(Args&&... args)
        {
            node_pointer p = create_node(std::forward<Args>(args)...);
            impl_.push_front(*p);
            return std::to_address(p)->value();
        }
        void pop_front()noexcept
        {
            erase(begin());
        }

        void resize(size_type n)
        {
            auto it = begin();
            while (it != end() && n != 0)
            {
                ++it;
                --n;
            }
            if (it != end())
            {
                erase(it, end());
                return;
            }
            for (; n != 0; --n)
            {
                emplace_back();     // 抛异常时已插入的元素保留（基本保证）
            }
        }
        void resize(size_type n, const T& value)
        {
            auto it = begin();
            while (it != end() && n != 0)
            {
                ++it;
                --n;
            }
            if (it != end())
            {
                erase(it, end());
                return;
            }
            for (; n != 0; --n)
            {
                emplace_back(value);
            }
        }

        void swap(list_without_size& other)noexcept(
            alloc_traits::propagate_on_container_swap::value
            || alloc_traits::is_always_equal::value)
        {
            if (this == &other)
            {
                return;
            }
            if constexpr (alloc_traits::propagate_on_container_swap::value)
            {
                using std::swap;
                swap(alloc_ref(), other.alloc_ref());
            }
            else
            {
                assert_compatible(other);
            }
            // basic_intrusive_list 不可复制/搬移（哨兵必须留在原地），
            // 借一条临时链把两边的节点整体换过去，仍然只改指针。
            list_type tmp;
            tmp.splice(tmp.end(), impl_);
            impl_.splice(impl_.end(), other.impl_);
            other.impl_.splice(other.impl_.end(), tmp);
        }

        // ===================== 链表操作 =====================
        void splice(const_iterator pos, list_without_size& other)noexcept
        {
            if (this == &other || other.empty())
            {
                return;
            }
            assert_compatible(other);
            impl_.splice(base_of(pos), other.impl_);
        }
        void splice(const_iterator pos, list_without_size&& other)noexcept
        {
            splice(pos, other);
        }
        void splice(const_iterator pos, list_without_size& other, const_iterator it)noexcept
        {
            assert_compatible(other);
            if (it == pos)
            {
                return;
            }
            auto next = it;
            ++next;
            if (pos == next)        // 插到自己后面等于原地不动
            {
                return;
            }
            list_type::splice(base_of(pos), other.impl_, base_of(it));
        }
        void splice(const_iterator pos, list_without_size&& other, const_iterator it)noexcept
        {
            splice(pos, other, it);
        }
        void splice(const_iterator pos, list_without_size& other, const_iterator first, const_iterator last)noexcept
        {
            if (first == last || pos == first)
            {
                return;
            }
            assert_compatible(other);
            list_type::splice(base_of(pos), other.impl_, base_of(first), base_of(last));
        }
        void splice(const_iterator pos, list_without_size&& other, const_iterator first, const_iterator last)noexcept
        {
            splice(pos, other, first, last);
        }

        size_type remove(const T& value)
        {
            return remove_if([&value](const T& v) { return v == value; });
        }
        template<class UnaryPred>
        size_type remove_if(UnaryPred pred)
        {
            // 节点已经 release() 之后才调用 deleter，由 deleter 负责销毁/归还节点
            return impl_.remove_if(
                [&pred](const node_base& n) { return pred(value_of(n)); },
                [this](node_base& n) { destroy_node(as_node(n)); });
        }
        size_type unique()
        {
            return unique(std::equal_to<T>{});
        }
        template<class BinaryPred>
        size_type unique(BinaryPred pred)
        {
            return impl_.unique(
                [&pred](const node_base& a, const node_base& b) { return pred(value_of(a), value_of(b)); },
                [this](node_base& n) { destroy_node(as_node(n)); });
        }

        void merge(list_without_size& other)
        {
            merge(other, std::less<T>{});
        }
        template<class Compare>
        void merge(list_without_size& other, Compare comp)
        {
            if (this == &other)
            {
                return;
            }
            assert_compatible(other);
            // 相等时保留 *this 的元素，因此稳定
            impl_.merge(other.impl_,
                [&comp](const node_base& a, const node_base& b) { return comp(value_of(a), value_of(b)); });
        }
        void merge(list_without_size&& other)
        {
            merge(other);
        }
        template<class Compare>
        void merge(list_without_size&& other, Compare comp)
        {
            merge(other, comp);
        }

        void sort()
        {
            sort(std::less<T>{});
        }
        template<class Compare>
        void sort(Compare comp)
        {
            // basic_intrusive_list::sort 是自底向上归并（稳定、O(n log n)、不分配），
            // 比较器抛异常时它会把节点全部拼回来，不会丢元素。
            impl_.sort([&comp](const node_base& a, const node_base& b)
                {
                    return comp(value_of(a), value_of(b));
                });
        }

        void reverse()noexcept
        {
            // 整条链的 prev/next 对调由 basic_intrusive_list::reverse 完成
            impl_.reverse();
        }

    private:
        list_type impl_;
    };

    template<class T, class Alloc>
    void swap(list_without_size<T, Alloc>& lhs, list_without_size<T, Alloc>& rhs)
        noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }
}
