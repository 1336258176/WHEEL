# thread

>  以下内容都是基于 `MSVC STL`

## 数据成员

```cpp
 _Thrd_t _Thr;

using _Thrd_id_t = unsigned int;
struct _Thrd_t { // thread identifier for Win32
    void* _Hnd; // Win32 HANDLE
    _Thrd_id_t _Id;
};
```

`_Thrd_t` 由于内存对齐，大小为16字节，需要注意的是 `libstdc++` 实现中只保有一个数据成员 `id _M_id` ，他的大小为8个字节

## 类基本函数

### 拷贝构造 拷贝赋值

```cpp
thread(const thread&)            = delete;
thread& operator=(const thread&) = delete;
```

拷贝构造和拷贝赋值弃置，`std::thread` 不可复制，且 两个 `std::thread` 不可表示同一个线程，`std::thread` 对线程资源是独占所有权的。

### 移动构造 移动赋值

```cpp
thread(thread&& _Other) noexcept : _Thr(_STD exchange(_Other._Thr, {})) {}

thread& operator=(thread&& _Other) noexcept {
    if (joinable()) {
        _STD terminate(); // per N4950 [thread.thread.assign]/1
    }

    _Thr = _STD exchange(_Other._Thr, {});
    return *this;
}
```

转移线程资源。`_STD` 是一个宏，展开就是 `::std::`，`std::exchange` 是一个标准库中的函数，表示给 `_Other._Thr` 赋空 `{}`，并返回旧值，此函数常用于移动构造和移动赋值。

### 构造函数

```cpp
thread() noexcept : _Thr{} {}

template <class _Fn, class... _Args, enable_if_t<!is_same_v<_Remove_cvref_t<_Fn>, thread>, int> = 0>
_NODISCARD_CTOR_THREAD explicit thread(_Fn&& _Fx, _Args&&... _Ax) {
    _Start(_STD forward<_Fn>(_Fx), _STD forward<_Args>(_Ax)...);
}
```

第一个默认构造函数，构造不关联线程的新 `std::thread` 对象，并对 `_Thr` 进行零初始化。

第二个带参数的构造函数模板，接受一个可调用类型 `_Fn` 和若干参数 `_Args`，使用 `SFINAE` 对传入的可调用类型 `_Fn` 进行约束，要求 `_Fn` 不能是 `thread`。构造函数内将参数完美转发到 `_Start` 函数中。

### `_Start`

```cpp
template <class _Fn, class... _Args>
void _Start(_Fn&& _Fx, _Args&&... _Ax) {
    using _Tuple                 = tuple<decay_t<_Fn>, decay_t<_Args>...>;
    auto _Decay_copied           = _STD make_unique<_Tuple>(_STD forward<_Fn>(_Fx), _STD forward<_Args>(_Ax)...);
    constexpr auto _Invoker_proc = _Get_invoke<_Tuple>(make_index_sequence<1 + sizeof...(_Args)>{});

    _Thr._Hnd =
        reinterpret_cast<void*>(_CSTD _beginthreadex(nullptr, 0, _Invoker_proc, _Decay_copied.get(), 0, &_Thr._Id));

    if (_Thr._Hnd) { // ownership transferred to the thread
        (void) _Decay_copied.release();
    } else { // failed to start thread
        _Thr._Id = 0;
        _Throw_Cpp_error(_RESOURCE_UNAVAILABLE_TRY_AGAIN);
    }
}
```

**函数逻辑：** 首先使用 `decay_t` 将所有参数退化并完美转发到由 `unique_ptr` 构建的 `tuple` 类型中 `_Decay_copied` ，接着利用 `make_index_sequence` 生成形参包索引序列传递给 `_Get_invoke`，生成实际运行的函数指针 `_Invoker_proc` ，最后将数据副本 `_Decay_copied` 和函数指针 `_Invoker_proc` 传递给 `Windows API _beginthreadex` ，该函数返回线程句柄同时构建线程id。

1. 为什么要使用 `unique_ptr` ？第一，**异常安全性**，它保证了程序崩溃时也能正常释放内存；第二，**便于转让所有权**

```cpp
if (_Thr._Hnd) { // ownership transferred to the thread
    (void) _Decay_copied.release();
}
```

若线程句柄获取成功则 `release`。关于 `unique_ptr` 的 `release` 函数，它仅代表释放被管理对象的所有权，即 `unique_ptr` 不再负责清理该对象，清理的职责交给 `release` 函数调用方。同时应当注意的是智能指针类的释放内存方法应是 `reset()`。

2. 为什么要使用 `std::tuple`？`std::tuple` 适合存储不同类型的值，这里使用 `tuple` 存储可调用类型和参数，然后利用 `index_sequence` 展开元组。
2. 如何实现的参数拷贝？通过将参数完美转发到 `tuple` 中，而对 `_Tuple` 的声名要求 `C++` 拷贝，即使是以引用传参。

```cpp
using _Tuple       = tuple<decay_t<_Fn>, decay_t<_Args>...>;
auto _Decay_copied = _STD make_unique<_Tuple>(_STD forward<_Fn>(_Fx), _STD forward<_Args>(_Ax)...);
```

若想在 `STL` 容器中实现对外的引用，则需要 `std::reference_wrapper`，使用示例如下：

```cpp
#include <iostream>
#include <type_traits>
#include <vector>

int main() {
  double d = 2.0;
  std::vector<std::reference_wrapper<double>> v{d};
  std::cout << d << '\n';
  v[0].get()++;
  std::cout << d << '\n';

  return 0;
}
```



### `_Invoke`

```cpp
template <class _Tuple, size_t... _Indices>
static unsigned int __stdcall _Invoke(void* _RawVals) noexcept /* terminates */ {
    // adapt invoke of user's callable object to _beginthreadex's thread procedure
    const unique_ptr<_Tuple> _FnVals(static_cast<_Tuple*>(_RawVals));
    _Tuple& _Tup = *_FnVals.get(); // avoid ADL, handle incomplete types
    _STD invoke(_STD move(_STD get<_Indices>(_Tup))...);
    _Cnd_do_broadcast_at_thread_exit(); // TRANSITION, ABI
    return 0;
}

template <class _Tuple, size_t... _Indices>
_NODISCARD static constexpr auto _Get_invoke(index_sequence<_Indices...>) noexcept {
    return &_Invoke<_Tuple, _Indices...>;
}
```

**函数逻辑：** `_Get_invoke` 用来生成 `_Invoke` 函数，利用模板的特性产生 `index_sequence` 展开元组，形参包展开统一传递给 `std::invoke`。

1. 为什么要将这两个函数声明为 `static`？因为调用类内成员函数，就必须提供实例化类对象的地址，否则就要将该成员函数声明为 `static`。
2. 为什么获取 `_Tup` 时要先声明一个 `unique_ptr` 再 `get` 得到？由于 `_RawVals` 是裸指针，需要一种机制确保它在使用完毕后被正确释放；使用 `unique_ptr` 后，`_FnVals.get()` 返回的引用 `_Tup` 可以避免 `ADL` 进行类型查找。这在某些情况下可以减少编译器的查找范围，从而避免潜在的命名冲突或意外行为。

`std::index_sequence` 使用示例：

```cpp
#include <iostream>
#include <utility>

template <std::size_t... I>
void f(std::index_sequence<I...>) {
  int _[]{(std::cout << I << ' ', 0)...};
}

int main() {
  f(std::make_index_sequence<10>{});

  return 0;
}
```

