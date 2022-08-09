## Deduced Context

* 复杂的类型声明的匹配过程从最顶层构造开始，然后不断递归子构造，即各种组成元素，这些构造被称为 deduced context，non-deduced context 不会参与推断，而是使用其他处推断的结果，受限类型名称如 `A<T>::type` 不能用来推断 T，非类型表达式如 `A<N + 1>` 不能用来推断 N

```cpp
namespace jc {

template <int N>
struct A {
  using T = int;

  void f(int) {}
};

template <int N>  // A<N>::T 是 non-deduced context，X<N>::*p 是 deduced context
void f(void (A<N>::*p)(typename A<N>::T)) {}

}  // namespace jc

int main() {
  using namespace jc;
  f(&A<0>::f);  // 由 A<N>::*p 推断 N 为 0，A<N>::T 则使用 N 变为 A<0>::T
}
```

## 特殊的推断情况

* 成员函数的推断

```cpp
namespace jc {

struct A {
  void f(int*) const noexcept {}
};

template <typename RT, typename T, typename... Args>
void f(RT (T::*)(Args...) const) {}

}  // namespace jc

int main() {
  jc::f(&jc::A::f);  // RT = void，T = A，Args = int*
}
```

* 取函数模板地址和调用转型运算符模板的推断

```cpp
namespace jc {

template <typename T>
void f(T) {}

struct A {
  template <typename T>
  operator T&() {
    static T x;
    return x;
  }
};

void g(int (&)[3]) {}

}  // namespace jc

int main() {
  void (*pf)(int) = &jc::f;  // 推断为 f<int>(int)

  jc::A a;
  jc::g(a);  // a 要转为 int(&)[3]，T 推断为 int[3]
}
```

* 初始化列表作为实参没有具体类型，不能直接推断为初始化列表
 
```cpp
#include <initializer_list>

namespace jc {

template <typename T>
void f(T) {}

template <typename T>
void g(std::initializer_list<T>) {}

}  // namespace jc

int main() {
  // jc::f({1, 2, 3});  // 错误：不能推断出 T 为 initializer_list
  jc::g({1, 2, 3});  // OK：T 为 int
}
```

* 参数包的推断

```cpp
namespace jc {

template <typename T, typename U>
struct A {};

template <typename T, typename... Args>
void f(const A<T, Args>&...);

template <typename... T, typename... U>
void g(const A<T, U>&...);

}  // namespace jc

int main() {
  using namespace jc;
  f(A<int, bool>{}, A<int, char>{});   // T = int, Args = [bool,char]
  g(A<int, bool>{}, A<int, char>{});   // T = [int, int], U = [bool, char]
  g(A<int, bool>{}, A<char, char>{});  // T = [int, char], U = [bool, char]
  // f(A<int, bool>{}, A<char, char>{});  // 错误，T 分别推断为 int 和 char
}
```

* 完美转发处理空指针常量时，整型值会被当作常量值 0

```cpp
#include <utility>

namespace jc {

constexpr int g(...) { return 1; }
constexpr int g(int*) { return 2; }

template <typename T>
constexpr int f(T&& t) {
  return g(std::forward<T>(t));
}

}  // namespace jc

static_assert(jc::f(0) == 1);
static_assert(jc::g(0) == 2);
static_assert(jc::f(nullptr) == 2);
static_assert(jc::g(nullptr) == 2);

int main() {}
```

## [SFINAE（Substitution Failure Is Not An Error）](https://en.cppreference.com/w/cpp/language/sfinae)

* SFINAE 用来禁止不相关函数模板在重载解析时造成错误

```cpp
template<typename T, unsigned N>
T* begin(T(&a)[N])
{
  return a;
}

template<typename C>
typename C::iterator begin(C& c)
{
  return c.begin();
}

int main()
{
  std::vector<int> v;
  int a[10];

  ::begin(v); // OK：只匹配第二个，因为第一个替换失败
  ::begin(a); // OK：只匹配第一个，因为第二个替换失败
}
```

* 当替换返回类型无意义时而忽略一个匹配，会造成编译器选择另一个更差的匹配

```cpp
// number of elements in a raw array:
template<typename T, unsigned N>
std::size_t len(T(&)[N])
{
  return N;
}

// number of elements for a type having size_type:
template<typename T>
typename T::size_type len(const T& t)
{
  return t.size();
}

// fallback for all other types:
std::size_t len(...) // 这个省略号是参数包
{
  return 0;
}

int a[10];
std::cout << len(a); // OK: len() for array is best match
std::cout << len("tmp"); //OK: len() for array is best match
std::vector<int> v;
std::cout << len(v); // OK: len() for a type with size_type is best match
int* p;
std::cout << len(p); // OK: only fallback len() matches
std::allocator<int> x;
std::cout << len(x); // ERROR: 2nd len() function matches best, but can't call size() for x
```

* 如果把 SFINAE 机制用于确保函数模板在某种限制下被忽略，我们就说 SFINAE out 这个函数，当在 C++ 标准中读到一个函数模板`不应该加入重载解析除非...`，就表示 SFINAE 被用来 SFINAE out 这个函数模板，比如 [std::thread](https://en.cppreference.com/w/cpp/thread/thread) 声明一个构造函数如下

```cpp
namespace std {
class thread {
 public:
  ...
  template<typename F, typename... Args>
  explicit thread(F&& f, Args&&... args);
  ...
};
}
```

* 并带有如下 remark

```cpp
Remarks: This constructor shall not participate in overload resolution if decay_t<F> is the same type as std::thread.
```

* 这表示这个模板构造函数被调用时，如果 [std::thread](https://en.cppreference.com/w/cpp/thread/thread) 作为第一个和唯一一个实参，则这个模板会被忽略。调用一个线程时，通过 SFINAE out 这个模板来保证预定义的拷贝或移动构造函数总被使用。标准库提供了禁用模板的工具，最出名的特性是 [std::enable_if](https://en.cppreference.com/w/cpp/types/enable_if)（它是用偏特化和 SFINAE 实现的）

```cpp
namespace std {
class thread {
 public:
  ...
  template<typename F, typename... Args,
    typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, thread>>>
  explicit thread(F&& f, Args&&... args);
  ...
};
}
```

* 指定正确的表达式来 SFINAE out 函数模板不会总是那么容易，比如想让函数模板 len() 对有 size_type 而没有 size() 的实参被忽略，之前的方案会产生错误

```cpp
template<typename T>
typename T::size_type len(const T& t)
{
  return t.size();
}

std::allocator<int> x;
std::cout << len(x) << '\n'; // ERROR: len() selected, but x has no size()
```

* 对此有一个惯用的解决方法
  * 使用尾置返回类型
  * 使用 decltype 和逗号运算符定义返回类型
  * 在逗号运算符开始处制定必须有效的全部表达式（转换为 void 以防逗号表达式被重载）
  * 在逗号运算符结尾处定义一个真正返回类型的对象

```cpp
template<typename T>
auto len(const T& t) -> decltype((void)(t.size()), T::size_type())
{
  return t.size();
}
```

* SFINAE 只发生于函数模板替换的即时上下文中。函数模板替换时发生于以下实例化期间的事，以及由替换过程触发的特殊成员函数的任何隐式定义，都不属于即时上下文的部分
  * 类模板定义
  * 函数模板定义
  * 变量模板初始化
  * 默认实参
  * 默认成员初始化
  * [异常规范（exception specification）](https://en.cppreference.com/w/cpp/language/except_spec)
* 如果替换时使用了类成员，则需要类模板实例化，此期间发生的错误就不在即时上下文中，即使另一个函数模板匹配无误也不会使用 SFINAE

```cpp
template<typename T>
class Array {
 public:
  using iterator = T*;
};

template<typename T>
void f(Array<T>::iterator first, Array<T>::iterator last);

template<typename T>
void f(T*, T*);

int main()
{
  f<int&>(0, 0); // 错误：在第一个模板中用 int& 替换 T 实例化 Array<int&>
}
```

* 使用 auto 返回类型，必须实例化定义来确定返回类型，实例化定义不属于即时上下文，此期间产生错误不会使用 SFINAE

```cpp
template<typename T>
auto f(T x)
{
  return x->m; // 实例化此定义时，x 为 int 则错误
}

int f(...); // 省略号参数的匹配不好

template<typename T>
auto g(T x) -> decltype(f(x));

int main()
{
  g(42); // 错误：第一个匹配更好，但实例化定义时出错，不会使用 SFINAE
}
```

## 推断的限制

* 被替换的参数类型可以是实参类型的基类

```cpp
template<typename T>
class B<T> {};

template<typename T>
class D : B<T> {};

template<typename T>
void f(B<T>*);

void g(D<long> x)
{
  f(&x); // 推断成功
}
```

* C++17 之前，模板实参推断不能用于类模板，不能从类模板的构造函数的实参推断类模板参数

```cpp
template<typename T>
class X {
 public:
  X(T b) : a(b) {}
 private:
  T a;
};

X x(12); // C++17 之前错误：不能从构造函数实参推断类模板参数 T
```

* C++17 允许[类模板实参推断](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction)，注意如果使用这种推断就不能显式指定一部分参数，类模板的所有参数要么通过显式指定指出，要么通过实参推断推出，不能一部分使用显式指定一部分使用推断

```cpp
template<typename T1, typename T2, typename T3 = T2>
class C {
 public:
  C(T1 x = T1{}, T2 y = T2{}, T3 z = T3{});
};

C c1(1, 3.14, "hi"); // OK：T1 = int，T2 = double，T3 = const char*
C c2(1, 3.14); // OK：T1 = int，T2 = T3 = double
C c3("hi", "guy"); // OK：T1 = T2 = T3 = const char*
C c4; // 错误：T1 和 T2 未定义
C c5("hi"); // 错误：T2 未定义
// 注意不能显式指定一部分而推断另一部分
C<string> c10("hi","my", 42); // 错误：只指定了 T1，T2 未推断
C<> c11(1, 3.14, 2); // 错误：T1 和 T2 都没指定
C<string, string> c12("hi","my"); // OK
```

* 默认实参不能用于推断

```cpp
template<typename T>
void f(T x = 42)
{}

int main()
{
  f<int>(); // OK: T = int
  f(); // 错误：不能用默认实参推断
}
```

* [异常规范（Exception Specification）](https://en.cppreference.com/w/cpp/language/except_spec)不参与模板实参推断

```cpp
template<typename T>
void f(T, int) noexcept(nonexistent(T()));

template<typename T>
void f(T, ...); // C-style var arg function

void test(int i)
{
  f(i, i); // 错误：选择第一个模板，但表达式 nonexistent(T()) 无效
}
```

* 同样的规则用于列出潜在的异常类型

```cpp
template<typename T>
void g(T, int) throw(typename T::Nonexistent); // C++11 弃用了 throw

template<typename T>
void g(T, ...);

void test(int i)
{
  g(i, i); // 错误：选择第一个模板，但类型 T::Nonexistent 无效
}
```

## 显式指定实参

* 指定空模板实参列表可以确保匹配模板

```cpp
int f(int);
template<typename T> T f(T);

auto x = f(42); // 调用函数
auto y = f<>(42); // 调用模板
```

* 在友元函数声明的上下文中，显式模板实参列表的存在有一个问题

```cpp
void f();

template<typename>
void f();

namespace N {
class C {
  friend int f(); // OK：在 N 中找不到 f，声明一个新的 f
  friend int f<>(); // 错误：查找到全局的模板 f，返回类型不一致
};
}
```

* 显式指定模板实参替换会使用 SFINAE

```cpp
template<typename T>
typename T::E f();

template<typename T>
T f();

auto x = f<int*>(); // 选择第二个模板
```

* 变参模板也能使用显式模板实参

```cpp
template<typename... Ts>
void f(Ts... args);

f<double, double, int>(1, 2, 3); // OK：1 和 2 转换为 double
g<double, int>(1, 2, 3);  // OK：模板实参是 <double, int, int>
```

## auto

* auto 使用了模板实参推断的机制，auto&& 实际是转发引用

```cpp
auto&& x = ...;
// 等价于
template<typename T>
void f(T&& x); // auto 被 T 替换

int i;
auto&& rr = 42; // auto 推断为 int，int&& rr = 42
auto&& lr = i; // auto 推断为 int&，int& lr = i
```

* auto 非类型模板参数可以用于参数化类成员模板

```cpp
template<typename>
struct PMClassT;

template<typename C, typename M>
struct PMClassT<M C::*> {
  using Type = C;
};

template<typename PM>
using PMClass = typename PMClassT<PM>::Type;

template<auto PMD>
struct CounterHandle {
  PMClass<decltype(PMD)>& c;
  CounterHandle(PMClass<decltype(PMD)>& c): c(c) {}

  void incr()
  {
    ++(c.*PMD);
  }
};

struct S {
  int i;
};

int main()
{
  S s{41};
  CounterHandle<&S::i> h(s);
  h.incr(); // 增加 s.i
}
```

* 使用 auto 模板参数只要指定指向成员的指针常量 `&S::i` 作为模板实参，而 C++17 前必须指定冗长的具体类型

```cpp
CounterHandle<int S::*, &S::i>
```

* 这个特性也能用于非类型参数包

```cpp
template<auto... VS>
struct Values {};

Values<1, 2, 3> beginning;
Values<1, 'x', nullptr> triplet;
```

* 强制一个同类型的非类型模板参数包

```cpp
template<auto V1, decltype(V1)... VRest>
struct X {};
```

## decltype

* 如果 e 是一个实例名称（如变量、函数、枚举、数据成员）或一个类成员访问， decltype(e) 产生实例或类成员实例的声明类型。当想匹配已有声明类型时

```cpp
auto x = ...;
auto y1 = x + 1; // y1 和 x 类型不一定相同，如果 x 是 char，y1 是 int
decltype(x) y2 = x + 1; // y2 和 x 类型一定相同
```

* 反之，如果 e 是其他表达式，decltype(e) 产生一个反映表达式 type 或 value 的类型
  * e 是 T 类型 lvalue，产生 T&
  * e 是 T 类型 xvalue，产生 T&&
  * e 是 T 类型 prvalue，产生 T

```cpp
int&& i = 0;
decltype(i) // int&&（i 的实体类型）
decltype((i)) // int&（i 的值类型）
```

## decltype(auto)

* C++14 允许 [decltype(auto)](https://en.cppreference.com/w/cpp/language/auto)

```cpp
int i = 42;
const int& r = i;
auto x = r; // int x = r
decltype(auto) y = r; // const int& y = r

std::vector<int> v = { 42 };
auto x2 = v[0]; // int x2 = v[0]，x2 是一个新对象
decltype(auto) y2 = v[0]; // int& y2 = v[0]，y2 是一个引用
```

* [decltype(auto)](https://en.cppreference.com/w/cpp/language/auto) 常用于返回类型

```cpp
template<typename C>
class X {
  C c;
  decltype(auto) operator[](std::size_t n)
  {
    return c[n];
  }
};
```

* 圆括号初始化会产生和 decltype 一样的影响

```cpp
int i;
decltype(auto) x = i; // int x = i
decltype(auto) r = (i); // int& r = i

int g() { return 1; }

decltype(auto) f()
{
  int r = g();
  return (r); // 返回临时变量的引用，应当避免此情况
}
```

* C++17 开始[decltype(auto)](https://en.cppreference.com/w/cpp/language/auto) 也可以用于推断非类型参数

```cpp
template<decltype(auto) Val>
class S {};

constexpr int c = 42;
extern int v = 42;

S<c> sc;   // produces S<42>
S<(v)> sv; // produces S<(int&)v>
```

* 在函数模板中使用可推断的非类型参数

```cpp
template<auto N>
struct S {};

template<auto N>
int f(S<N> p);

S<42> x;
int r = f(x);
```

* 也有许多不能被推断的模式

```cpp
template<auto V>
int f(decltype(V) p);

int r1 = deduce<42>(42); // OK
int r2 = deduce(42); // 错误：decltype(V) 是一个non-deduced context
```

## auto 推断的特殊情况

* 模板参数不能推断为初始化列表推断

```cpp
template<typename T>
void f(T);

f({ 1 }); // 错误
f({ 1, 2, 3 }); // 错误

template<typename T>
void g(std::initializer_list<T>);
g({ 1, 2, 3 }); // OK：T 推断为 int
```

* 对初始化列表的推断，auto 会将其视为 std::initializer_list，即上述第一种情形

```cpp
auto x = { 1, 2 }; // x 是 initializer_list<int>
```

* C++14 禁止了对 auto 用 initializer_list 直接初始化，必须用=

```cpp
auto x  { 1, 2 }; // 错误
```

* 但允许单个元素的直接初始化，这和圆括号初始化一样，不会被推断为 std::initializer_list

```cpp
auto x { 1 }; // x 为 int，和 auto x(1) 效果一样
```

* 返回类型为 auto 时不能返回一个初始化列表

```cpp
auto f() { return { 1 }; } // 错误
```

* 共享 auto 的变量必须有相同推断类型

```cpp
char c;
auto *p = &c, d = c; // OK
auto e = c, f = c + 1; // 错误：e 为 char，f 为 int

auto f(bool b)
{
  if (b)
  {
    return 42.0; // 返回类型推断为 double
  }
  else
  {
    return 0; // 错误：推断不一致
  }
}
```

* 如果返回的表达式递归调用函数，则不会发生推断从而导致出错

```cpp
// 错误例子
auto f(int n)
{
  if (n > 1)
  {
    return n * f(n - 1); // 错误：f(n - 1) 类型未知
  }
  else
  {
    return 1;
  }
}

// 正确例子
auto f(int n)
{
  if (n <= 1)
  {
    return 1; // OK：返回类型被推断为 int
  }
  else
  {
    return n * f(n - 1); // OK：f(n - 1) 为 int，所以 n * f(n - 1) 也为 int
  }
}
```

* auto 返回类型没有对应的副本时会推断为 void，若不能匹配 void 则出错

```cpp
auto f1() {} // OK：返回类型是 void
auto f2() { return; } // OK：返回类型是 void
auto* f3() {} // 错误：auto*不能推断为 void
```

* 当考虑到 SFINAE 会有一些意料外的情况

```cpp
template<typename T, typename U>
auto f(T t, U u) -> decltype(t+u)
{
  return t + u;
}

void f(...);

template<typename T, typename U>
auto g(T t, U u) -> decltype(auto)  // 必须实例化 t 和 u 来确定返回类型
{
  return t + u;  // 此处的实例化在定义中，不是即时上下文，不适用 SFINAE
}

void g(...);

struct X {};

using A = decltype(f(X(), X())); // OK：A 为 void
using B = decltype(g(X(), X())); // 错误：g<X, X> 的实例化非法
```

## 结构化绑定（Structured Binding）

* 结构化绑定是 C++17 引入的新特性，作用是在一次声明中引入多个变量。一个结构化绑定必须总有一个 auto，可以用 cv 限定符或 &、&& 声明符

```cpp
struct X { bool valid; int value; };
X g();
const auto&& [b, N] = g(); // 把 b 和 N 绑定到 g() 的结果的成员
```

* 初始化一个结构化绑定，除了使用类类型，还可以使用数组、std::tuple-like 的类（通过 get<> 绑定）

```cpp
// 数组的例子
double pt[3];
auto& [x, y, z] = pt;
x = 3.0; y = 4.0; z = 0.0;

// 另一个例子
auto f() -> int(&)[2];  // f() 返回一个 int 数组的引用

auto [ x, y ] = f(); // auto e = f(), x = e[0], y = e[1]
auto& [ r, s ] = f(); // auto& e = f(), x = e[0], y = e[1]

// std::tuple 的例子
std::tuple<bool, int> bi {true, 42};
auto [b, i] = bi; // auto b = get<0>(bi), i = get<1>(bi)
int r = i; // int r = 42
```

* 对于 tuple-like 类 E（或它的表达式，设为 e），如果[std::tuple_size\<E>::value](https://en.cppreference.com/w/cpp/utility/tuple/tuple_size) 是一个有效的整型常量表达式，它必须等于中括号标识符的数量。如果表达式 e 有名为 get 的成员，表现如下

```cpp
std::tuple_element<i, E>::type& n_i = e.get<i>();
// 如果 e 推断为引用类型
std::tuple_element<i, E>::type&& n_i = e.get<i>();

// 如果 e 没有 get 成员
std::tuple_element<i, E>::type& n_i = get<i>(e);
std::tuple_element<i, E>::type&& n_i = get<i>(e);
```

* 特化 [std::tuple_size](https://en.cppreference.com/w/cpp/utility/tuple/tuple_size) 和 [std::tuple_element](https://en.cppreference.com/w/cpp/utility/tuple/tuple_element) 并使用 [std::get(std::tuple)](https://en.cppreference.com/w/cpp/utility/tuple/get) 即可生成一个 tuple-like 类

```cpp
#include <utility>

enum M {};

template<>
struct std::tuple_size<M> {
  static unsigned const value = 2; // 将 M 映射为一对值
};

template<>
struct std::tuple_element<0, M> {
  using type = int; // 第一个值类型为 int
};

template<>
struct std::tuple_element<1, M> {
  using type = double; // 第二个值类型为 double
};

template<int> auto get(M);
template<> auto get<0>(M) { return 42; }
template<> auto get<1>(M) { return 7.0; }

auto [i, d] = M(); // 相当于 int&& i = 42, double&& d = 7.0
```

## 泛型 lambada（Generic Lambda）

* C++14 中 lambda 的参数类型可以为 auto

```cpp
template<typename Iter>
Iter findNegative(Iter first, Iter last)
{
  return std::find_if(first, last,
  [] (auto value) {
    return value < 0;
  });
}
```

* lambda 创建时不知道实参类型，推断不会立即进行，而是先把模板类型参数添加到模板参数列表中，这样 lambda 就可以被任何实参类型调用，只要实参类型支持 `<0` 操作，结果能转为 bool

```cpp
[] (int i) {
  return i < 0;
}
```

* 编译器把这个表达式编译成一个新创建类的实例，这个实例称为闭包（closure）或闭包对象（closure object），这个类称为闭包类型（closure type）。闭包类型有一个函数调用运算符（function call operator），因此闭包是一个函数对象。上面这个 lambda 的闭包类型就是一个编译器内部的类。如果检查一个 lamdba 的类型，[std::is_class](https://en.cppreference.com/w/cpp/types/is_class) 将生成 true

```cpp
// 上述的 lambda 相当于下面类的一个默认构造对象的简写
class X { // 一个内部类
 public:
  X(); // 只被编译器调用
  bool operator() (int i) const
  {
    return i < 0;
  }
};


foo(..., [] (int i) { return i < 0; });
// 等价于
foo(..., X{}); // 传递一个闭包类型对象
```

* 如果 lambda 要捕获局部变量，捕获将被视为关联类成员的初始化

```cpp
int x, y;
[x,y] (int i) {
  return i > x && i < y;
}
// 将编译为下面的类
class Y {
 public:
  Y(int x, int y) : _x(x), _y(y) {}
  bool operator() (int i) const
  {
    return i > _x && i < _y;
  }
 private:
  int _x, _y;
};
```

* 对一个泛型 lambda，函数调用操作符将变成一个成员函数模板，因此

```cpp
[] (auto i) {
  return i < 0;
}
// 将编译为下面的类
class Z {
 public:
  Z();
  template<typename T>
  auto operator() (T i) const
  {
    return i < 0;
  }
};
```

* 当闭包被调用时才会实例化成员函数模板，而不是出现 lambda 的位置。下面的 lambda 出现在 main 函数中，创建一个闭包，但直到把闭包和两个 int 传递给 invoke（即 invoke 实例化时），闭包的函数调用符才被实例化

```cpp
#include <iostream>

template<typename F, typename... Ts>
void invoke(F f, Ts... ps)
{
  f(ps...);
}

int main()
{
  ::invoke([](auto x, auto y) {
    std::cout << x + y << '\n';
  },
  21, 21);
}
```

## 别名模板（Alias Template）

* 无论带有模板实参的别名模板出现在何处，别名的定义都会被实参替代，产生的模式将用于推断

```cpp
template<typename T, typename Cont>
class Stack;

template<typename T>
using DequeStack = Stack<T, std::deque<T>>;

template<typename T, typename Cont>
void f1(Stack<T, Cont>);

template<typename T>
void f2(DequeStack<T>);

template<typename T>
void f3(Stack<T, std::deque<T>); // 等价于 f2

void test(DequeStack<int> intStack)
{
  f1(intStack); // OK：T 推断为int，Cont 推断为 std::deque<int>
  f2(intStack); // OK：T 推断为int
  f3(intStack); // OK：T 推断为int
}
```

* 别名模板不能被特化

```cpp
template<typename T>
using A = T;

template<>
using A<int> = void; // 错误
```

## Deduction Guide

* C++17 引入了 deduction guide，它用于将一个模板名称声明为一个类型标识符，通过 deduction guide 不需要使用名称查找，而是使用模板实参推导，一个模板的所有 deduction guide 都会作为推导依据

```cpp
explicitopt template-name (parameter-declaration-clause) -> simple-template-id;
```

* deduction guide 有点像函数模板，但语法上有一些区别
  * 看起来像尾置返回类型的部分不能写成传统的返回类型，这个类型（即上例的`S<T>`）就是 guided type
  * 尾置返回类型前没有 auto 关键字
  * deduction guide 的名称必须是之前在同一作用域声明的类模板的非受限名称
  * guide 的 guided type 必须是一个 template-id，其 template 对应 guide name
  * 能被 explicit 限定符声明
* 使用 deduction guide 即可令特定实参类型推断为指定类型

```cpp
template<typename T>
struct X {
  T i;
};

X(const char*) -> X<std::string>;

int main()
{
  X x{ "hello" }; // T 推断为 std::string
  std::cout << x.i;
}
```

* 将 deduction guide 用于类模板推断

```cpp
template<typename T>
class X {
 public:
  X(T b) : a(b) {}
 private:
  T a;
};

template<typename T> X(T) -> X<T>;

int main()
{
  X x{1}; // X<int> x{1}
  X y(1); // X<int> y(1)
  auto z = X{12}; // auto z = X<int>{1}
  X xx(1), yy(2.0); // 错误：X 推断为 X<int> 和 X<double>
}
```

* 在 `X x{1}` 中的限定符X称为一个占位符类类型，其后必须紧跟一个变量名和初始化

```cpp
X* p = &x; // 语法错误
```

* 使用花括号赋值可以解决没有初始化列表的问题，圆括号则不行

```cpp
template<typename T>
struct X {
  T val;
};

template<typename T> X(T)->X<T>;

int main()
{
  X x1{42}; // OK
  X x2 = {42}; // OK
  X x3(42); // 错误：没有初始化列表，int 不能转为 X<int>
  X x4 = 42; // 错误：没有初始化列表，int 不能转为 X<int>
}
```

* explicit 声明的 deduction guide 只用于直接初始化

```cpp
template<typename T, typename U>
struct X {
  X(const T&);
  X(T&&);
};

template<typename T> X(const T&) -> X<T, T&>;
template<typename T> explicit X(T&&) -> X<T, T>; // 只有直接初始化能使用

X x1 = 1; // 只使用非 explicit 声明的 deduction guide：X<int, int&> x1 = 1
X x2{2}; // 第二个 deduction guide 更合适：X<int, int> x2{2}
```

## 隐式的 Deduction Guide

* C++17 中的[类模板实参推断](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction)本质是为类模板的每个构造函数和构造函数模板隐式添加了一个 deduction guide

```cpp
template<typename T>
class X {
 public:
  X(T b) : a(b) {}
 private:
  T a;
};

// template<typename T> X(T) -> X<T> // 隐式 deduction guide
```

* deduction guide有一个歧义

```cpp
X x{12}; // x 类型为 X<int>
X y{x}; // y 类型为 X<int> 还是 X<X<int>>？
X z(x); // z 类型为 X<int> 还是 X<X<int>>？
```

* 标准委员会有争议地决定两个都是 `X<int>` 类型，这个争议造成的问题如下

```cpp
std::vector v{1, 2, 3};
std::vector v1{v}; // vector<int>
std::vector v2{v, v}; // vector<vector<int>>
```

* 编程中很容易错过这种微妙之处

```cpp
template<typename T, typename... Ts>
auto f(T x, Ts... args)
{ // 如果 T 推断为 vector
  std::vector v{ x, args... };  // 参数包是否为空将决定不同的 v 类型
}
```

* 添加隐式 deduction guide 是有争议的，主要反对观点是这个特性自动将接口添加到已存在的库中。把上面的类模板定义修改如下，隐式 deduction guide 还会失效

```cpp
template<typename T>
struct A {
  using Type = T;
};

template<typename T>
class X {
 public:
  using ArgType = typename A<T>::Type;
  X(ArgType b) : a(b) {}
 private:
  T a;
};

// template<typename T> X(typename A<T>::Type) -> X<T>; // 隐式 deduction guide
// 该 deduction guide 无效，因为有限定名称符 A<T>::
int main()
{
  X x{1}; // 错误
}
```

## Deduction Guide 的其他细微问题

* 为了保持向后兼容性，如果模板名称是[注入类名](https://en.cppreference.com/w/cpp/language/injected-class-name)，则禁用[类模板实参推断](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction)

```cpp
template<typename T>
struct X {
  template<typename U> X(U x);
  template<typename U>
  auto f(U x)
  {
    return X(x); // 根据注入类名规则 X 是 X<T>，根据类模板实参推断 X 是 X<U>
  }
};
```

* 使用转发引用的 deduction guide 可能导致预期外的结果（推断出引用类型，导致实例化错误或产生空悬引用），标准委员会因此决定使用隐式 deduction guide 的推断时，禁用 T&& 这个特殊的推断规则

```cpp
template<typename T>
struct X {
  X(const T&);
  X(T&&);
};

// 如果把隐式 deduction guide 指定出来就将出错
template<typename T> Y(const T&) -> Y<T>; // (1)
template<typename T> Y(T&&) -> Y<T>; // (2)

void f(std::string s)
{
  X x = s; // 预期想通过隐式 deduction guide 推断 T 为 std::string
  // (1) 推断 T 为 std::string，但要求实参转为 const std::string
  // (2) 推断 T 为 std::string&，是一个更好的匹配，这是预期外的结果
}
```

* 拷贝构造和初始化列表的问题

```cpp
template<typename ... Ts>
struct Tuple {
  Tuple(Ts...);
  Tuple(const Tuple<Ts...>&);
};

// 隐式 deduction guide
template<typename... Ts> Tuple(Ts...) -> Tuple<Ts...>; // (1)
template<typename... Ts> Tuple(const Tuple<Ts...>&) -> Tuple<Ts...>; // (2)

int main()
{
  auto x = Tuple{1, 2}; // 明显使用 (1)，x 是 Tuple<int, int>
  Tuple a = x; // (1) 为 Tuple<Tuple<int, int>，(2) 为 Tuple<int, int>，(2) 匹配更好
  Tuple b(x); // 和 a 一样推断为 Tuple<int, int>，a 和 b 都由 x 拷贝构造
  Tuple c{x, x}; // 只能匹配 (1)，生成 Tuple<Tuple<int, int>, Tuple<int, int>>
  Tuple d{x}; // 看起来和 c 的匹配一样，但会被视为拷贝构造，匹配 (2)
  auto e = Tuple{x}; // 和 d 一样，推断为 Tuple<int, int> 而非 <Tuple<int, int>>
}
```

* deduction guide 不是函数模板，它们只用于推断而非调用，实参的传递方式对 deduction guide 声明不重要

```cpp
template<typename T>
struct X {};

template<typename T>
struct Y {
  Y(const X<T>&);
  Y(X<T>&&);
};

template<typename T> Y(X<T>) -> Y<T>; // 虽然不对应构造函数但没有关系
// 对于一个 X<T> 类型的值 x 将选用可推断类型 Y<T>
// 随后初始化并对 Y<T> 的构造函数进行重载解析
// 根据 x 是左值还是右值决定调用的构造函数
```
