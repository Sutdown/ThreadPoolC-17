#include <iostream>
#include<thread>
#include<mutex>

#include <chrono> // 处理时间相关的操作
#include<ctime>

#include<vector>
#include<queue>
#include<string>

#include<future> // 处理异步操作
#include <functional>
#include <utility> // 提供通用工具类和函数，比如右值引用，完美转发等
#include <condition_variable> //用于线程同步，阻塞线程
#include< shared_mutex>
using namespace std;

// 积木1：实现多线程安全的任务队列
// 使用C++17中的共享锁实现类似读写所的作用
template<typename T>
struct safe_queue {
    queue<T> que;
    shared_mutex _m; // 互斥锁，支持共享锁个独占锁。
    // 允许同时读取，但是不能写入
    
    bool empty() {
        // 共享锁来锁定互斥量_m
        shared_lock<shared_mutex>lc(_m);
        return que.empty();
    }
    
    auto size() {
        unique_lock<shared_mutex> lc(_m);
        return que.size();
    }
    
    void push(T& t) {
        // 独占锁来锁定互斥量_m，保证只有一个线程能写入
        unique_lock<shared_mutex> lc(_m);
        que.push(t);
    }
    
    bool pop(T& t) {
        unique_lock<shared_mutex> lc(_m);
        if (que.empty())return false;
        // 右值引用，避免的队列元素的拷贝，利用的是移动复制而不是拷贝构造的思想
        t = move(que.front());
        que.pop();
        return true;
    }
};

// 积木2：线程池
// 通过复用一定数量的线程减少频繁创建和销毁线程的开销。
class ThreadPool {
private:

    class worker {
    public:
        ThreadPool* pool;
        worker(ThreadPool* _pool) : pool{ _pool } { }

        // 定义了每个线程的主要执行逻辑
        // 允许线程池中的每个线程不断从任务队列中取出任务并执行，直到队列为空或者线程池关闭
        void operator ()() {
            while (!pool->is_shut_down) {
                {
                    unique_lock<mutex> lock(pool->_m);
                    // 条件变量的一个重载方法，使得当前线程进入等待状态
                    // 直到线程池关闭或者任务队列不为空时 结束等待
                    pool->cv.wait(lock, [this]() {
                        return this->pool->is_shut_down ||
                        !this->pool->que.empty();
                        });
                }

                //如果能够成功去除一个任务，就执行它。
                function<void()>func;
                bool flag = pool->que.pop(func);
                if (flag) func();
            }
        }
    };

public:

    bool is_shut_down;
    safe_queue<std::function<void()>> que; // 队列
    vector<std::thread>threads; // 线程
    mutex _m; // 互斥锁
    condition_variable cv;  // 条件变量

    // 构造函数
    ThreadPool(int n) : threads(n), is_shut_down{ false } {
        for (auto& t : threads) t = thread{ worker(this) };
    }

    // 禁止了ThreadPool对象的拷贝构造，移动构造，拷贝赋值和移动赋值操作
    // 原因：ThreadPool管理线程，任务队列和同步原语。
    // 1.如果允许拷贝或者移动，ThreadPool必须正确处理所有资源，防止资源泄露或者发生竞争条件。
    // 2.ThreadPool的实例应该是唯一的，多个实例共享或者争夺资源会引发错误。
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /*
    * submit函数通过function和packaged_task将任务和它的返回值封装，
    * 然后提交到线程池执行。调用者可以通过future异步获取任务执行结果。
    * 允许用户灵活提交各种类型的任务，避免手动管理线程和任务的复杂性。
    */
    template <typename F, typename... Args>
    // future: 表示异步操作的结果
    // 在不阻塞主线程的情况下启动异步任务，在未来某个时刻获取该任务结果
    // future和packaged_task一起使用
    auto submit(F&& f, Args &&... args) -> std::future<decltype(f(args...))> {
        
        // 封装任务
        function<decltype(f(args...))()> func = [&f, args...]() {return f(args...); };
        // 创建任务对象
        /*packaged_task作用：
        * std::packaged_task 将一个可调用对象包装起来，并将其与 std::future 绑定。
        * 包装的任务可以在不同的线程中异步执行，而主线程或其他线程可以通过 std::future 获取任务的结果
        * 通过 std::packaged_task，可以在任务执行后，使用 std::future 对象来获取任务的返回值或处理异常。
        */
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
       
        // 包装任务
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)(); // 这个是对它指向的packaged_task对象的调用，变成了无参数的函数对象
        };

        que.push(warpper_func);
        cv.notify_one();

        return task_ptr->get_future();
    }

    // 析构函数
    ~ThreadPool() {
        // 主要目的是确保所有的工作线程都能接收到一个任务，以便随后被唤醒进行处理
        // 保证线程处理完当前任务
        auto f = submit([]() {});
        f.get();
        is_shut_down = true;
        cv.notify_all(); // 通知，唤醒所有工作线程
        // 保证每个线程正常退出，阻塞和安全检查
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }
};

mutex _m;

/* 整体流程：
* 1.线程池初始化
* 2.提交任务
* 3.任务执行
* 4.互斥锁保护
*/
int main()
{
    ThreadPool pool(8);
    int n = 20;
    for (int i = 1; i <= n; i++) {
        pool.submit([](int id) {
            // 模拟延迟
            if (id % 2 == 1) {
                this_thread::sleep_for(0.2s);
            }

        unique_lock<mutex> lc(_m);
        cout << "id : " << id << endl;
            }, i);
    }
}