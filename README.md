* C++ [模板](https://en.cppreference.com/w/cpp/language/templates)技术是泛型编程的核心，但囿于编译器技术限制，不得不带着缺陷诞生，语法晦涩，报错冗长，难以调试，应用层开发较少使用，相关技术书籍匮乏，因此掌握难度较大。模板相关的经典技术书籍主要有三本，分别是 2001 年出版的 [*Modern C++ Design*](https://book.douban.com/subject/1755195/)、2002 年出版的 [*C++ Templates*](https://book.douban.com/subject/1455780/)、2004 年出版的 [*C++ Template Metaprogramming*](https://book.douban.com/subject/1920800/)。三者基于的 C++ 标准都是 C++98，*Modern C++ Design* 涉及 [Andrei Alexandrescu](https://en.wikipedia.org/wiki/Andrei_Alexandrescu) 写书时配套的 [Loki](http://loki-lib.sourceforge.net/)，*C++ Template Metaprogramming* 涉及 [Boost](https://www.boost.org/)，二者以介绍元编程（模板技术的一种应用）为主，只有 *C++ Templates* 主要介绍 C++98 标准的模板技术。时过境迁，C++ 标准的更新逐步修复了一些语法缺陷，减少了使用者的心智负担，并引入了语法糖和工具，让编写模板越来越简单。2017 年 9 月 25 日，基于 C++17 标准，[*C++ Templates 2ed*](https://book.douban.com/subject/11939436/) 出版，填补了十多年间模板技术进化时相关书籍的空白，堪称最全面的模板教程，也是对 C++11/14/17 特性介绍最为全面的书籍之一。个人完整学习[原书](https://www.safaribooksonline.com/library/view/c-templates-the/9780134778808/)后，梳理精简章节脉络，补充 C++20 相关特性，如 [concepts](https://en.cppreference.com/w/cpp/concepts)、支持模板参数的 [lambda](https://en.cppreference.com/w/cpp/language/lambda) 等，运行验证所有代码结果，最终记录至此。

## Basics

1. [Function template](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/01_function_template.md)
2. [Class template](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/02_class_template.md)
3. [Non-type template parameter](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/03_non_type_template_parameter.md)
4. [Variadic template](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/04_variadic_template.md)
5. [Move semantics and perfect forwarding](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/05_move_semantics_and_perfect_forwarding.md)
6. [Name lookup](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/06_name_lookup.md)
7. [Instantiation](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/07_instantiation.md)
8. [Template argument deduction](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/08_template_argument_deduction.md)
9. [Specialization and overloading](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/09_specialization_and_overloading.md)

## Designing

10. [Traits](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/10_traits.md)
11. [Overloading on type property](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/11_overloading_on_type_property.md)
12. [Inheritance](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/12_inheritance.md)
13. [Bridging static and dynamic polymorphism](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/13_bridging_static_and_dynamic_polymorphism.md)
14. [Metaprogramming](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/14_metaprogramming.md)
15. [Typelist](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/15_typelist.md)
16. [Tuple](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/16_tuple.md)
17. [Variant](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/17_variant.md)
18. [Expression template](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/18_expression_template.md)
19. [Debugging](https://github.com/downdemo/Cpp-Templates-2ed/blob/master/docs/19_debugging.md)
