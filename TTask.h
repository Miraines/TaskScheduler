#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <stdexcept>

const size_t INVALID_TASK_ID = static_cast<size_t>(-1);

class TaskID {
 public:
  TaskID() : id_(INVALID_TASK_ID) {}
  TaskID(size_t id) : id_(id) {}
  TaskID(const TaskID& other) : id_(other.id_) {}

  TaskID& operator=(const TaskID& other) {
    if (this != &other) {
      id_ = other.id_;
    }
    return *this;
  }

  bool operator==(const TaskID& other) const {
    return id_ == other.id_;
  }

  bool operator!=(const TaskID& other) const {
    return id_ != other.id_;
  }

  operator size_t() const {
    return id_;
  }

  bool isValid() const {
    return id_ != INVALID_TASK_ID;
  }

 private:
  size_t id_;
};

// Специализация std::hash для TaskID
namespace std {
template <>
struct hash<TaskID> {
  std::size_t operator()(const TaskID& taskID) const {
    return std::hash<size_t>()(static_cast<size_t>(taskID));
  }
};
}

class Any {
 public:
  Any() : content(nullptr) {}

  template<typename T>
  Any(const T& value) : content(new Holder<T>(value)) {}

  Any(const Any& other) : content(other.content ? other.content->clone() : nullptr) {}

  Any& operator=(const Any& other) {
    if (this != &other) {
      delete content;
      content = other.content ? other.content->clone() : nullptr;
    }
    return *this;
  }

  ~Any() {
    delete content;
  }

  template<typename T>
  T& any_cast() {
    assert(content);
    auto* holder = dynamic_cast<Holder<T>*>(content);
    if (!holder) {
      throw std::bad_cast();
    }
    return holder->held;
  }

 private:
  struct Placeholder {
    virtual ~Placeholder() = default;
    virtual Placeholder* clone() const = 0;
  };

  template<typename T>
  struct Holder : public Placeholder {
    Holder(const T& value) : held(value) {}
    Placeholder* clone() const override {
      return new Holder(held);
    }
    T held;
  };

  Placeholder* content;
};

class ITask {
 public:
  virtual ~ITask() = default;
  virtual Any execute() = 0;
};

class TTaskScheduler;

template<typename Func, typename Arg1>
class Task1 : public ITask {
 public:
  Task1(Func func, Arg1 arg1) : func_(func), arg1_(arg1) {}

  Any execute() override {
    return func_(arg1_);
  }

 private:
  Func func_;
  Arg1 arg1_;
};

template<typename Func, typename Arg1, typename Arg2>
class Task2 : public ITask {
 public:
  Task2(TTaskScheduler& scheduler, Func func, Arg1 arg1, Arg2 arg2);

  Any execute() override;

 private:
  TTaskScheduler& scheduler_;
  Func func_;
  Arg1 arg1_;
  Arg2 arg2_;
};

class TTaskScheduler {
 public:
  template <typename Func, typename Arg1>
  TaskID add(Func func, Arg1 arg1) {
    TaskID id(tasks_.size());
    tasks_.emplace_back(std::make_shared<Task1<Func, Arg1>>(func, arg1));
    return id;
  }

  template <typename Func, typename Arg1, typename Arg2>
  TaskID add(Func func, Arg1 arg1, Arg2 arg2) {
    TaskID id(tasks_.size());
    auto task = std::make_shared<Task2<Func, Arg1, Arg2>>(*this, func, arg1, arg2);
    tasks_.emplace_back(task);

    // Добавление зависимости, если второй аргумент - TaskID
    if constexpr (std::is_same_v<Arg2, TaskID>) {
      if (!arg2.isValid()) {
        throw std::invalid_argument("Task ID is not initialized.");
      }
      addDependency(id, arg2);
    }

    return id;
  }

  template <typename T>
  struct FutureResult {
    FutureResult(TTaskScheduler& scheduler, TaskID id) : scheduler_(scheduler), id_(id) {
      if (!id_.isValid()) {
        throw std::invalid_argument("Task ID is not initialized.");
      }
    }

    operator T() {
      return scheduler_.getResult<T>(id_);
    }

   private:
    TTaskScheduler& scheduler_;
    TaskID id_;
  };

  template <typename T>
  FutureResult<T> getFutureResult(TaskID id) {
    if (!id.isValid()) {
      throw std::invalid_argument("Task ID is not initialized.");
    }
    return FutureResult<T>(*this, id);
  }

  template <typename T>
  T getResult(TaskID id) {
    if (!id.isValid()) {
      throw std::invalid_argument("Task ID is not initialized.");
    }
    if (results_.find(id) == results_.end()) {
      if (id >= tasks_.size()) {
        throw std::out_of_range("Task ID does not exist.");
      }
      executeTask(id);
    }
    return results_[id].any_cast<T>();
  }

  void executeAll() {
    std::unordered_map<TaskID, bool> visited;
    for (size_t i = 0; i < tasks_.size(); ++i) {
      TaskID id(i);
      if (!visited[id]) {
        executeTask(id, visited);
      }
    }
  }

 private:
  std::vector<std::shared_ptr<ITask>> tasks_;
  std::unordered_map<TaskID, Any> results_;
  std::unordered_map<TaskID, std::vector<TaskID>> dependencies_;

  void executeTask(TaskID id, std::unordered_map<TaskID, bool>& visited) {
    if (visited[id]) return;

    visited[id] = true;
    for (TaskID dep : dependencies_[id]) {
      if (!visited[dep]) {
        executeTask(dep, visited);
      }
    }

    if (results_.find(id) == results_.end()) {
      results_[id] = tasks_[id]->execute();
    }
  }

  void executeTask(TaskID id) {
    std::unordered_map<TaskID, bool> visited;
    executeTask(id, visited);
  }

  void addDependency(TaskID task, TaskID dependency) {
    dependencies_[task].push_back(dependency);
  }

  // Даем Task2 доступ к приватным членам TTaskScheduler
  template<typename Func, typename Arg1, typename Arg2>
  friend class Task2;
};

template<typename Func, typename Arg1, typename Arg2>
Task2<Func, Arg1, Arg2>::Task2(TTaskScheduler& scheduler, Func func, Arg1 arg1, Arg2 arg2)
    : scheduler_(scheduler), func_(func), arg1_(arg1), arg2_(arg2) {}

template<typename Func, typename Arg1, typename Arg2>
Any Task2<Func, Arg1, Arg2>::execute() {
  if constexpr (std::is_same_v<Arg2, TaskID>) {
    auto resolvedArg2 = scheduler_.getResult<typename std::invoke_result<Func, Arg1, size_t>::type>(arg2_);
    return func_(arg1_, resolvedArg2);
  } else {
    return func_(arg1_, arg2_);
  }
}
