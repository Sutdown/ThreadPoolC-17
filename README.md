> 实现多线程安全的任务队列，线程池使用异步操作，提交(submit)使用与thread相同。
> 内部利用**完美转发**获取可调用对象的函数签名，lambda与function包装任务，使用RAII管理线程池的生命周期。



Thread Pool：

一种用于管理和复用一组工作线程的设计模式，通过创建一个固定数量的线程，并将需要执行的任务提交到线程中执行，从而避免为每个任务单独创建和销毁线程开销。

**main：**

整体流程：

1. 线程池初始化

2. 提交任务

3. 任务执行

4. 互斥锁保护


**积木1：安全队列**

**积木2：线程池**

- [移动构造函数C++11](https://avdancedu.com/a39d51f9/)
- [完美转发](https://zhuanlan.zhihu.com/p/369203981) 完美转发=万能引用+引用折叠+forward C++11
- [函数，函数指针，仿函数，lambda](https://zhuanlan.zhihu.com/p/561916691)
- [什么是RALL](https://zhuanlan.zhihu.com/p/600337719)
- functor，状态变量
- 禁止拷贝构造，移动构造，拷贝赋值，移动赋值
- function和packaged_task，future
