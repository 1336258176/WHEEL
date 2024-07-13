#include <iostream>
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