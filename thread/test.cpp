#include <iostream>
#include <thread>
#include "myThread.hpp"

void print_test(int num) {
  std::cout << num << '\n';
}

struct X {
  void f(double) {
    std::cout << "X: f()\n";
  }
};

void test(double& d) {
  std::cout << "ori: " << d << '\n';
  d += 1.0;
  std::cout << "aft: " << d << '\n';
}

int main() {
  std::cout << sizeof(std::thread) << '\n'; // MSVC: 16 Libstdc++: 8 实现不同
  std::cout << sizeof(myThread) << '\n';

  myThread t{print_test, 1};
  myThread t2{ [] {
    std::cout << "lambda\n";
  }};

  X x{};
  myThread t3{&X::f, x, 1.};

  double a = 1.0;
  myThread t4{test, std::ref(a)};

  return 0;
}