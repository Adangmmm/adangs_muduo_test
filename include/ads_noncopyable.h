#pragma once

/*
 * noncopyable 是一个基类，用于禁止对象的拷贝构造和拷贝赋值操作。
 * 通过继承该类，可以确保派生类对象不能被拷贝或赋值。
 * 这种做法在某些情况下是非常有用的，尤其是像资源管理类（如套接字类、文件流类等），它们通常不希望被复制。
 */

class noncopyable{
public:
    // 禁止拷贝构造
    noncopyable(const noncopyable &) = delete; 
    // 禁止赋值构造
    noncopyable &operator=(const noncopyable &) = delete;
    // void operator=(const noncopyable &) = delete;    // muduo将返回值变为void 这其实无可厚非

protected:
    // protected 表示 noncopyable 类的构造函数和析构函数是保护的。这意味着只有继承自 noncopyable 的类（或其友元类）可以访问这些构造和析构函数。
    // 这确保了派生类能够正常创建对象并进行析构，但不能进行拷贝构造或赋值。
    noncopyable() = default;
    ~noncopyable() = default;
 };
