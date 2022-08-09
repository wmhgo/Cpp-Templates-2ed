## 重载模板以适用不同的情况

* 下面用于交换两个对象的函数模板 exchange 可以处理简单类型，但如果 T 是下面的类，就没必要再拷贝一次对象并调用两次赋值运算符，而只需要使用成员模板 exchangeWith 交换内部的指针

```cpp
template<typename T>
class Array {
 public:
  Array(const Array<T>&);
  Array<T>& operator=(const Array<T>&);
  void exchangeWith(Array<T>* b)
  {
    T* tmp = data;
    data = b->data;
    b->data = tmp;
  }
  T& operator[](std::size_t k)
  {
    return data[k];
  }
 private:
  T* data;
};

template<typename T> inline
void exchange(T* a, T* b)
{
  T tmp(*a);
  *a = *b;
  *b = tmp;
}
```

* 但使用成员模板的用户要记忆并在适当情况下使用这个接口，为了免去这个负担，重载函数模板即可实现一致性

```cpp
template<typename T> inline
void quick_exchange(T* a, T* b) // (1)
{
  T tmp(*a);
  *a = *b;
  *b = tmp;
}

template<typename T> inline
void quick_exchange(Array<T>* a, Array<T>* b) // (2)
{
  a->exchange_with(b);
}

void demo(Array<int>* p1, Array<int>* p2)
{
  int x = 42, y = -7;
  quick_exchange(&x, &y); // 使用(1)
  quick_exchange(p1, p2); // 使用(2)：重载会优先选择更加特殊的模板
}
```

* 虽然上述两种交换的算法都能交换指针指向的值，但各自的副作用截然不同

```cpp
struct S {
  int x;
} s1, s2;

void distinguish(Array<int> a1, Array<int> a2)
{
  a1[0] = 1;
  a2[0] = 2;
  s1.x = 3;
  s2.x = 4;
  int* p = &a1[0]; // p 和 a1->data 指向同一处，此处存储的值为 1
  int* q = &s1.x; // q 指向 s1.x，s1.x 的值为 3
  quick_exchange(&a1, &a2); // a1->data 和 a2->data 交换了，现在换 a2->data 和 p 指向 1
  quick_exchange(&s1, &s2); // s1 和 s2 的值交换了，s1.x 现在为 4，所以 *q 为 4
}
```

* 如果不明白参考下面代码

```cpp
int main()
{
  int* a = new int[3];
  a[0] = 1;
  int* p = &a[0]; // 把 a 现在的地址赋值给 p，之后 a 指向哪都不会改变 p

  int* b = new int[3];
  b[0] = 2;
  int* q = b;
  b = a; // 此时 abp 都指向一处
  a = q; // a 指向了原来 b 的那处，bp 指向一处
  std::cout << a[0] <<  b[0] << *p; // 211
}

// 更简单的例子
int main()
{
  int i = 1, j = 2;
  int* p = &i; // 把 i 的地址赋值给 p
  int* q = p; // 把 p 的值，即 i 的地址赋值给 q
  p = &j; // p 的值改为 j 的地址，但 q 的值仍为 i 的地址
  std::cout << *q; // 1
}
```

* 如果不使用成员模板 exchangeWith，可以将函数改为递归调用（因此不再使用 inline）

```cpp
template<typename T>
void quick_exchange(T* a, T* b)
{
  T tmp(*a);
  *a = *b;
  *b = tmp;
}

template<typename T>
void quick_exchange(Array<T>* a, Array<T>* b)
{
  T* p = &(*a)[0];
  T* q = &(*b)[0];
  for (std::size_t k = a->size(); k-- != 0; )
  {
    quick_exchange(p++, q++); // 如果 T 也是 Array 则递归调用此模板
  }
}
```

## 签名（Signature）

* 只要具有不同的签名，两个函数就能同时存在于同一个程序中，函数签名（简单理解就是函数声明中所包含的信息）定义如下
  * 函数的非受限名称
  * 函数名称所属的作用域
  * 函数的 cv 限定符
  * 函数的 & 或 && 限定符
  * 函数参数类型
  * 函数模板还包括返回类型、模板参数和模板实参
* 以下模板具有不同的签名，其实例化体可以在一个程序中同时存在

```cpp
template<typename T1, typename T2>
void f(T1, T2);

template<typename T1, typename T2>
void f(T2, T1);

template<typename T>
long g(T);

template<typename T>
char g(T);
```

* 但上述模板在同一作用域中时的实例化可能导致二义性

```cpp
#include <iostream>

template<typename T1, typename T2>
void f(T1, T2) {}

template<typename T1, typename T2>
void f(T2, T1) {}

int main()
{
  f<char, char>('a', 'b'); // 二义性错误
}
```

* 只有两个模板出现在不同的编译单元，两个实例化体才能在一个程序中同时存在

```cpp
// Translation unit 1:
#include <iostream>

template<typename T1, typename T2>
void f(T1, T2)
{
  std::cout << 1;
}

void g()
{
  f<char, char>('a', 'b');
}

// Translation unit 2:
#include <iostream>

template<typename T1, typename T2>
void f(T2, T1)
{
  std::cout << 2;
}

extern void g(); // defined in translation unit 1

int main()
{
  f<char, char>('a', 'b'); // 2
  g(); // 1
}
```

## 函数模板的偏序（Partial Ordering）规则

* 多个函数模板匹配实参列表时，C++ 规定了偏序规则来决定调用哪个模板，之所以叫偏序是因为一些模板可以是一样特殊的，此时则会出现二义性调用

```cpp
#include <iostream>

template<typename T>
void f(T)
{
  std::cout << 1;
}

template<typename T>
void f(T*)
{
  std::cout << 2;
}

int main()
{
  f<int*>((int*)nullptr); // 1：重载解析
  f<int>((int*)nullptr); // 2：重载解析
  f(0); // 1：0 被推断为 int，无重载解析，匹配第一个模板
  f(nullptr); // 1
  f((int*)nullptr); // 2：重载解析，第二个更特殊
}
```

* 决定哪个模板更特殊的规则为：考虑两个模板 T1 和 T2，不考虑省略号参数和默认实参，用假想的类型 X 替换 T1，看 T2 是否能推断 T1 的参数列表（忽略隐式转换），再把 T1 和 T2 反过来做一次这个过程，能被推断的那个就是更特殊的。由这个规则推出的一些结论：特化比泛型特殊、`T*` 比 T 特殊、`const T` 比 T 特殊、`const T*` 比 `T*` 特殊

```cpp
template<typename T>
void f(T*, const T* = nullptr, ...);

template<typename T>
void f(const T*, T*, T* = nullptr);

void example(int* p)
{
  f(p, p); // 错误：二义性调用，两个模板没有偏序关系，一样特殊
}
```

## 普通函数会被重载优先考虑

* 函数模板也可以和非模板的普通函数重载，其他条件相同时优先调用非模板函数

```cpp
template<typename T>
void f(T)
{
  std::cout << 1;
}

void f(int&)
{
  std::cout << 2;
}

int main()
{
  int x = 0;
  f(x); // 2
}
```

* 当有 const 和引用限定符时重载解析会改变

```cpp
template<typename T>
void f(T)
{
  std::cout << 1;
}

void f(const int&)
{
  std::cout << 2;
}

int main()
{
  int x = 0;
  f(x); // 1
  const int y = 0;
  f(y); // 2
}
```

* 但这对特殊的成员模板会造成意外的行为

```cpp
class C {
 public:
  C() = default;
  C(const C&) { std::cout << 1; }
  C(C&&) { std::cout << 2; }
  template<typename T>
  C(T&&) { std::cout << 3; }
};

int main()
{
  C x;
  C x2{x}; // 3：对于 non-const，成员模板是比拷贝构造函数更好的匹配
  C x3{std::move(x)}; // 2
  const C c;
  C x4{c}; // 1
  C x5{std::move(c)}; // 3：对于 const C&&（尽管不常见），成员模板比移动构造函数更好
}
```

## 变参函数模板的重载

```cpp
#include <iostream>

template<typename T>
void f(T*)
{
  std::cout << 1;
}

template<typename... Ts>
void f(Ts...)
{
  std::cout << 2;
}

template<typename... Ts>
void f(Ts*...)
{
  std::cout << 3;
}

int main()
{
  f(0, 0.0); // 2
  f((int*)nullptr, (double*)nullptr); // 3
  f((int*)nullptr); // 1
}
```

* 包扩展同理

```cpp
#include <iostream>

template<typename... Ts>
class Tuple {};

template<typename T>
void f(Tuple<T*>)
{
  std::cout << 1;
}

template<typename... Ts>
void f(Tuple<Ts...>)
{
  std::cout << 2;
}

template<typename... Ts>
void f(Tuple<Ts*...>)
{
  std::cout << 3;
}

int main()
{
  f(Tuple<int, double>()); // 2
  f(Tuple<int*, double*>()); // 3
  f(Tuple<int*>()); // 1
}
```

## 函数模板全特化

* 函数模板特化引入了重载和实参推断，如果能推断特化版本，就可以不显式声明模板实参

```cpp
template<typename T>
int f(T) // (1)
{
  return 1;
}

template<typename T>
int f(T*) // (2)
{
  return 2;
}

template<>
int f(int) // OK：(1) 的特化
{
  return 3;
}

template<>
int f(int*) // OK：(2) 的特化
{
  return 4;
}
```

* 函数模板特化不能有默认实参，但会使用要被特化的模板的默认实参

```cpp
template<typename T>
int f(T, T x = 42)
{
  return x;
}

template<>
int f(int, int = 35) // 错误
{
  return 0;
}

template<typename T>
int g(T, T x = 42)
{
  return x;
}

template<>
int g(int, int y)
{
  return y/2;
}

int main()
{
  std::cout << g(0) << std::endl; // 21
}
```

* 特化声明的不是一个模板，非内联的函数模板特化在同个程序中的定义只能出现一次，通常应该把特化的实现写在源文件中。如果想定义在头文件内，可以把特化声明为内联函数

```cpp
#ifndef TEMPLATE_G_HPP
#define TEMPLATE_G_HPP

// 模板定义应放在头文件中
template<typename T>
int g(T, T x = 42)
{
  return x;
}

// 特化声明会阻止模板实例化
// 为避免重定义错误不在此定义
template<>
int g(int, int y);

#endif // TEMPLATE_G_HPP

// 实现文件
#include "template_g.hpp"
template<>
int g(int, int y)
{
  return y/2;
}
```

## 类模板全特化

* 特化的实参列表必须对应模板参数，非类型值不能替换类型参数，如果有默认实参可以不指定对应参数

```cpp
template<typename T>
class Types {
 public:
  using I = int;
};

template<typename T, typename U = typename Types<T>::I>
class X; // (1)

template<>
class X<void> { // (2)使用默认实参：X<void, int>
 public:
  void f();
};

template<> class X<char, char>; // (3)

template<> class X<char, 0>; // 错误：0 不能替换 U

int main()
{
  X<int>* pi; // OK：使用 (1)，不需要定义
  X<int> e1; // 错误：使用 (1) 但没有定义
  X<void>* pv; // OK：使用 (2)
  X<void, int> sv; // OK：使用 (2)
  X<void, char> e2; // 错误：使用 (1) 但没有定义
  X<char, char> e3; // 错误：使用 (3) 但没有定义
}

template<>
class X<char, char> {} // (3) 的定义
```

* 类特化和泛型模板的唯一区别是，特化不能单独出现，必须有一个模板的前置声明。特化声明不是模板声明，所以类外定义成员时应该使用普通的定义语法（即不指定 `template<>` 前缀）

```cpp
template<typename T>
class X; // 必须有此前置声明才能特化

template<>
class X<char**> {
 public:
  void print() const;
};

// 下面的定义不能使用 template<> 前缀
void X<char**>::print() const
{
  std::cout << "pointer to pointer to char\n";
}

// 另一个例子
template<typename T>
class A {
 public:
  template<typename U>
  class B {};
};

template<>
class A<void> {
  // 下面的嵌套类和上面定义的泛型模板之间并不存在联系
  template<typename U>
  class B {
   private:
    static int i;
  };
};

// 下面的定义不能使用 template<> 前缀
template<typename U>
int A<void>::B<U>::i = 1;
```

* 可以用全特化代替对应的某个实例化体，但两者不能在一个程序中同时存在，否则会发生编译期错误

```cpp
template<typename T>
class X {};

X<double> x; // 产生一个 X<double> 实例化体

template<>
class X<double>; // 错误：X<double> 已经被实例化了
```

* 如果在不同的编译单元出现这种情况很难捕捉错误，如果没有特殊目的应该避免让模板特化来源于外部资源。下面是一个无效的例子

```cpp
// Translation unit 1:
template<typename T>
class X {
 public:
  enum { max = 10; };
};

char buffer[X<void>::max]; // 使用的 max 是 10

extern void f(char*);

int main()
{
  f(buffer);
}

// Translation unit 2:
template<typename T>
class X;

template<>
class X<void> {
 public:
  enum { max = 100; };
};

void f(const char* buf)
{
  // 可能与原先定义的数组大小不匹配
  for (int i = 0; i < X<void>::max; ++i)
  {
    buf[i] = '\0';
  }
}
```

## 成员全特化

```cpp
template<typename T>
class A {
 public:
  template<typename U>
  class B {
   private:
    static int x;
  };

  static int y;

  void print() const
  {
    std::cout << "generic";
  }
};

template<typename T>
int A<T>::y = 6;

template<typename T>
  template<typename U>
int A<T>::B<U>::x = 7;

// A<bool> 的特化，对特化整个类模板可以完全改变类的成员
template<>
class A<bool> {
 public:
  template<typename U>
  class B {
   private:
    static int x;
  };

  void print() const {}
};

// 由于 A<bool> 中的 B 也是类模板，所以也可以特化
template<> // 特化的普通成员不需要加 template<> 前缀，但 B 是模板
class A<bool>::B<wchar_t> {
 public:
  enum { x = 2 };
};

// 下面是 A<void> 成员的特化，A<void> 其他成员将来自原模板
// 成员特化后就不能再特化整个 A<void>
template<>
int A<void>::y = 12;

template<>
void A<void>::print() const
{
  std::cout << "A<void>";
}
// template<> class A<void> {}; // 错误：不能再特化 A<void>

// 特化 A<wchar_t>::B
template<>
  template<typename U>
class A<wchar_t>::B {
 public:
  static long x; // 成员类型发生了改变
};

template<>
  template<typename X>
long A<wchar_t>::B<X>::x;

// 特化 A<char>::B<wchar_t>
template<> // A<char> 没被特化，所以比特化 A<bool>::B<wchar_t> 多一个前缀
  template<>
class A<char>::B<wchar_t> {
 public:
  enum { x = 1 };
};
```

* 普通类的成员函数和静态成员变量在类外的非定义声明是非法的

```cpp
class A {
  void f();
};

void A::f(); // 错误
```

* 但成员特化可以在类外声明

```cpp
template<typename T>
class A {
 public:
  template<typename U>
  class B {
   private:
    static int x;
  };

  static int y;

  void print() const
  {
    std::cout << "generic";
  }
};

template<>
int A<void>::y;

template<>
void A<void>::print() const;
```

* 如果静态成员变量只能用默认构造函数初始化，就不能定义它的特化

```cpp
class X {
 public:
  X() = default;
  X(const X&) = delete;
};

template<typename T>
class Y {
 private:
  static T i;
};

// 下面只是一个声明
template<>
X Y<X>::i;
// 下面是定义（C++11 前不存在定义的方法）
template<>
X Y<X>::i{};
// C++11前
template<>
X Y<X>::i = X(); // 但这不可行，因为拷贝构造函数被删除了
// 但 C++17 引入了 copy-elision 规则又使得这个方法可行
```

## 类模板偏特化

* 类模板偏特化限定一些类型而非某个具体类型

```cpp
template<typename T>
class X {}; // 原始模板

template<typename T>
class X<const T> {}; // T为 const 的情况

template<typename T>
class X<T*> {}; // T 为指针的情况

template<typename T, int N> // 参数个数可以和原始模板不一致
class X<T[N]> {}; // T 为数组的情况

template<typename A>
class X<void* A::*> {}; // T 为成员指针且成员返回类型为 void 的情况

template<typename T, typename A>
class X<T* A::*> {}; // T 为成员指针的情况（如果为 void 则调用上一个偏特化）

template<int I, int N>
class S {};

template<int N>
class S<2, N> {}; // I 为 2 的情况

template<typename... Ts>
class Tuple {}; // 原始模板

template<typename T>
class Tuple<T> {}; // tuple 只有单个元素的情况

template<typename T1, typename T2, typename... Ts>
class Tuple<T1, T2, Ts...> {}; // tuple 有两个以上元素的情况
```

* 偏特化可能产生无限递归，解决方法是在偏特化前提供一个全特化，匹配时全特化会优于偏特化

```cpp
template<typename T>
class X {};

template<>
class X<void*> {}; // 加上此全特化以防 X<void*> 产生无限递归

template<typename T>
class X<T*> {
 public:
  X<void*> x; // X<void*> 中又包含 X<void*>，将产生无限递归
};
```

* 下面是一些错误的偏特化

```cpp
template<typename T, int I = 3>
class X {}; // 原始模板

template<typename T>
class X<int, T> {}; // 错误：参数类型不匹配

template<typename T = int>
class X<T, 10> {}; // 错误：不能有默认实参（可以用原始模板的默认实参）

template<int I>
class X<int, I*2> {}; // 错误：不能有非类型表达式

template<typename U, int K>
class X<U, K> {}; // 错误：与原始模板相同

template<typename... Ts>
class Tuple {}; 

template<typename T, typename... Ts>
class Tuple<Ts..., T> {}; // 错误：包扩展必须在实参列表末尾

template<typename T, typename... Ts>
class Tuple<Tuple<Ts...>, T> {}; // OK：包扩展在嵌套模板实参列表末尾
```

* 偏特化会匹配更特殊的版本，如果匹配程度一样就会出现二义性错误

```cpp
template<typename T>
class X {};

template<typename T>
class X<const T> {};

template<typename T, int N>
class X<T[N]> {};

X<const int[3]> x; // 错误：两个偏特化匹配程度相同
```

* 偏特化的类外成员定义和类模板的方式一样

```cpp
template<typename T>
class X {};

template<typename T>
class X<T*> {
 public:
  void f();
};

template<typename T>
void X<T*>::f() {}
```

## 变量模板全特化

```cpp
template<typename T>
constexpr std::size_t X = sizeof(T);

template<>
constexpr std::size_t X<void> = 0;
```

* 变量模板特化的类型可以不匹配原始模板

```cpp
template<typename T>
typename T::iterator null_iterator; // 一个空迭代器（但不是 nullptr）

template<>
int* null_iterator<std::vector<int>> = nullptr; // 允许 int* 不匹配T::iterator

auto p = null_iterator<std::vector<int>>; // int* p = nullptr
auto q = null_iterator<std::deque<int>>; // std::deque<int>::iterator q = 空迭代器
```

## 变量模板偏特化

```cpp
template<typename T>
constexpr std::size_t X = sizeof(T);

template<typename T>
constexpr std::size_t X<T&> = sizeof(void*);
```

* 和变量模板的全特化一样，偏特化的类型不需要和原始模板匹配

```cpp
template<typename T>
typename T::iterator null_iterator;

template<typename T, std::size_t N>
T* null_iterator<T[N]> = nullptr;

auto p = null_iterator<int[3]>; // int* p = nullptr
auto q = null_iterator<std::deque<int>>; // std::deque<int>::iterator q = 空迭代器
```
