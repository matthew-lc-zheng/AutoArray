#pragma once
#include <atomic>
#include <functional>
template <typename T> class UniqueArray {
public:
  UniqueArray(T *array_ptr)
      : _array(array_ptr), _deleter([](T *ptr) { delete[] ptr; }) {}
  UniqueArray(T *array_ptr, std::function<void(T *)> deleter)
      : _array(array_ptr), _deleter(deleter) {}
  ~UniqueArray() { release(); }

  UniqueArray(UniqueArray &&other) noexcept
      : _array(other._array), _deleter(other._deleter) {
    other._array = nullptr;
  }

  UniqueArray &operator=(UniqueArray &&other) noexcept {
    if (this != &other) {
      release();
      _array = other._array;
      _deleter = other._deleter;
      other._array = nullptr;
    }
    return *this;
  }
  T &operator[](size_t index) { return _array[index]; }
  const T &operator[](size_t index) const { return _array[index]; }
  T **operator&() { return &_array; }
  const T **operator&() const { return &_array; }
  T &operator*() { return *_array; }
  const T &operator*() const { return *_array; }

  void reset(T *ptr) {
    release();
    _array = ptr;
    _deleter = [](T *ptr) { delete[] ptr; };
  }
  void reset(T *ptr, std::function<void(T *)> deleter) {
    release();
    _array = ptr;
    _deleter = deleter;
  }
  T *get() const { return _array; }

private:
  UniqueArray() {}
  UniqueArray(const UniqueArray &) {}
  UniqueArray &operator=(const UniqueArray &) = delete;
  void release() {
    if (_array) {
      _deleter(_array);
      _array = nullptr;
    }
  }

  T *_array;
  std::function<void(T *)> _deleter;
};

struct SharedControl {
  std::atomic<uint64_t> counter = 1;
};

template <typename T> class SharedArray {
public:
  SharedArray(T *ptr)
      : _array(ptr), _deleter([](T *ptr) { delete[] ptr; }),
        _control(new SharedControl) {}
  SharedArray(T *ptr, std::function<void(T *)> deleter)
      : _array(ptr), _deleter(deleter), _control(new SharedControl) {}
  SharedArray(const SharedArray &other) {
    other._control->counter.fetch_add(1, std::memory_order_relaxed);
    _control = other._control;
    _array = other._array;
    _deleter = other._deleter;
  }
  SharedArray(SharedArray &&other) {
    _control = other._control;
    _array = other._array;
    _deleter = other._deleter;
    other._control = nullptr;
    other._array = nullptr;
  }
  ~SharedArray() { release(); }

  SharedArray &operator=(const SharedArray &other) {
    other._control->counter.fetch_add(1, std::memory_order_relaxed);
    _control = other._control;
    _array = other._array;
    _deleter = other._deleter;
    return *this;
  }
  SharedArray &operator=(SharedArray &&other) {
    control = other._control;
    _array = other._array;
    _deleter = other._deleter;
    other._control = nullptr;
    other._array = nullptr;
    return *this;
  }
  T &operator[](size_t index) { return _array[index]; }
  const T &operator[](size_t index) const { return _array[index]; }
  T **operator&() { return &_array; }
  const T **operator&() const { return &_array; }
  T &operator*() { return *_array; }
  const T &operator*() const { return *_array; }

  void reset(T *ptr) {
    release();
    _array = ptr;
    _deleter = [](T *ptr) { delete[] ptr; };
    _control = new SharedControl;
  }
  void reset(T *ptr, std::function<void(T *)> deleter) {
    release();
    _array = ptr;
    _deleter = deleter;
    _control = new SharedControl;
  }
  T *get() const { return _array; }

private:
  SharedArray() {}

  void release() {
    _control->counter.fetch_sub(1, std::memory_order_relaxed);
    if (0 < _control->counter.load()) {
      return;
    }
    if (_array) {
      _deleter(_array);
      _array = nullptr;
    }
    delete _control;
    _control = nullptr;
  }

  T *_array;
  std::function<void(T *)> _deleter;
  SharedControl *_control;
};