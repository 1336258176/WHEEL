#ifndef MY_THREAD__
#define MY_THREAD__

#include <functional>
#include <memory>
#include <utility>

class myThread {
 public:
  myThread(const myThread&) = delete;
  myThread& operator=(const myThread&) = delete;

  myThread(myThread&& other) noexcept : id_(std::exchange(other.id_, {})) {}
  myThread& operator=(myThread&& other) noexcept {
    id_ = std::exchange(other.id_, {});
    return *this;
  }

 private:
  // 此函数相当于 windows API 中的 _beginthreadex，用来启动线程
  // 本质就是传递函数指针和参数，然后调用此函数
  // 使用 static 模拟处于别的库的函数
  static void start_thread(void (*start_address)(void*), void* args) {
    start_address(args);
  }

  template <typename Tuple, std::size_t... idx>
  static void Invoker(void* ral_val) {
    std::unique_ptr<Tuple> _tup(static_cast<Tuple*>(ral_val));
    Tuple& tup = *_tup.get();
    std::invoke(std::move(std::get<idx>(tup))...);
  }

  template <typename Tuple, std::size_t... idx>
  static constexpr auto getInvoker(std::index_sequence<idx...>) {
    return &Invoker<Tuple, idx...>;
  }

  template <typename Func, typename... Args>
  void start(Func&& func, Args&&... args) {
    using Tuple = std::tuple<std::decay_t<Func>, std::decay_t<Args>...>;
    auto Decay_copy = std::make_unique<Tuple>(std::forward<Func>(func), std::forward<Args>(args)...);
    auto invoke_proc = getInvoker<Tuple>(std::make_index_sequence<1 + sizeof...(Args)>{});
  
    //若想调用 invoke_proc，则必须将 getInvoke 和 Invoker 声名为 static
    start_thread(invoke_proc, Decay_copy.release());  
  }

 public:
  template <typename Func, typename... Args>
    requires requires { !std::is_same_v<std::remove_cvref_t<Func>, myThread>; }
  explicit myThread(Func&& func, Args&&... args) {
    start(std::forward<Func>(func), std::forward<Args>(args)...);
  }

  myThread() noexcept : id_{} {}

 private:
  using id = unsigned int;
  id id_{};
};

#endif  // MY_THREAD__