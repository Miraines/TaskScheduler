#include <iostream>
#include <gtest/gtest.h>
#include "TTask.h"


int simpleFunc(int x) {
  return x + 1;
}

int addFunc(int x, int y) {
  return x + y;
}

int multiplyFunc(int x, int y) {
  return x * y;
}

TEST(TTaskSchedulerTest, AddTaskWithSingleArgument) {
  TTaskScheduler scheduler;
  TaskID id = scheduler.add(simpleFunc, 5);
  EXPECT_TRUE(id.isValid());
}

TEST(TTaskSchedulerTest, AddTaskWithTwoArguments) {
  TTaskScheduler scheduler;
  TaskID id = scheduler.add(addFunc, 5, 10);
  EXPECT_TRUE(id.isValid());
}

TEST(TTaskSchedulerTest, AddTaskWithInvalidTaskID) {
  TTaskScheduler scheduler;
  TaskID invalidID;
  try {
    TaskID id = scheduler.add(addFunc, 5, invalidID);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& e) {
    EXPECT_EQ(e.what(), std::string("Task ID is not initialized."));
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }
}

TEST(TTaskSchedulerTest, GetResultForAddedTask) {
  TTaskScheduler scheduler;
  TaskID id = scheduler.add(simpleFunc, 5);
  int result = scheduler.getResult<int>(id);
  EXPECT_EQ(result, 6);
}

TEST(TTaskSchedulerTest, GetResultForTaskWithDependencies) {
  TTaskScheduler scheduler;
  TaskID id1 = scheduler.add(simpleFunc, 5);
  // Используем getResult для получения результата выполнения id1
  int result1 = scheduler.getResult<int>(id1);
  TaskID id2 = scheduler.add(addFunc, 3, result1);
  // Используем getResult для получения результата выполнения id2
  int result2 = scheduler.getResult<int>(id2);
  EXPECT_EQ(result2, 9); // Исправляем результат
}

TEST(TTaskSchedulerTest, GetResultForInvalidTaskID) {
  TTaskScheduler scheduler;
  TaskID invalidID;
  try {
    int result = scheduler.getResult<int>(invalidID);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& e) {
    EXPECT_EQ(e.what(), std::string("Task ID is not initialized."));
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }
}

TEST(TTaskSchedulerTest, ExecuteAllTasks) {
  TTaskScheduler scheduler;
  TaskID id1 = scheduler.add(simpleFunc, 5);
  TaskID id2 = scheduler.add(simpleFunc, 10);
  scheduler.executeAll();
  int result1 = scheduler.getResult<int>(id1);
  int result2 = scheduler.getResult<int>(id2);
  EXPECT_EQ(result1, 6);
  EXPECT_EQ(result2, 11);
}

TEST(TTaskSchedulerTest, ExecuteAlreadyExecutedTasks) {
  TTaskScheduler scheduler;
  TaskID id1 = scheduler.add(simpleFunc, 5);
  scheduler.executeAll();
  int result1 = scheduler.getResult<int>(id1);
  EXPECT_EQ(result1, 6);
  // Повторное выполнение должно быть безопасным
  scheduler.executeAll();
  int result2 = scheduler.getResult<int>(id1);
  EXPECT_EQ(result2, 6);
}

TEST(TTaskSchedulerTest, GetResultForNonExistentTask) {
  TTaskScheduler scheduler;
  try {
    TaskID nonExistentID(999); // Задача с этим ID не существует
    int result = scheduler.getResult<int>(nonExistentID);
    FAIL() << "Expected std::out_of_range";
  } catch (const std::out_of_range& e) {
    EXPECT_STREQ(e.what(), "Task ID does not exist.");
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

TEST(TTaskSchedulerTest, GetResultForTaskWithMultipleDependencies) {
  TTaskScheduler scheduler;
  TaskID id1 = scheduler.add(simpleFunc, 5);
  TaskID id2 = scheduler.add(simpleFunc, 10);
  TaskID id3 = scheduler.add(addFunc, 3, id2);
  TaskID id4 = scheduler.add(addFunc, scheduler.getFutureResult<int>(id1), id3);
  scheduler.executeAll();
  int result1 = scheduler.getResult<int>(id1);
  int result2 = scheduler.getResult<int>(id2);
  int result3 = scheduler.getResult<int>(id3);
  int result4 = scheduler.getResult<int>(id4);
  EXPECT_EQ(result1, 6);
  EXPECT_EQ(result2, 11);
  EXPECT_EQ(result3, 14);
  EXPECT_EQ(result4, 20);
}

TEST(TTaskSchedulerTest, FutureResultTest) {
  TTaskScheduler scheduler;
  TaskID id = scheduler.add(simpleFunc, 5);
  auto future = scheduler.getFutureResult<int>(id);
  scheduler.executeAll();
  int result = future;
  EXPECT_EQ(result, 6);
}

TEST(TTaskSchedulerTest, FutureResultWithInvalidTaskID) {
  TTaskScheduler scheduler;
  TaskID invalidID;
  try {
    auto future = scheduler.getFutureResult<int>(invalidID);
    int result = future; // Ожидаем исключение при попытке получить результат
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& e) {
    EXPECT_EQ(e.what(), std::string("Task ID is not initialized."));
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }
}

TEST(TTaskSchedulerTest, FutureResultTypeTest) {
  TTaskScheduler scheduler;
  TaskID id = scheduler.add(simpleFunc, 5);
  auto future = scheduler.getFutureResult<int>(id);
  static_assert(std::is_same<decltype(future), TTaskScheduler::FutureResult<int>>::value, "FutureResult type mismatch");
}

TEST(TTaskSchedulerTest, GetResultForAlreadyExecutedTask) {
  TTaskScheduler scheduler;
  TaskID id = scheduler.add(simpleFunc, 5);
  scheduler.executeAll();
  int result1 = scheduler.getResult<int>(id);
  EXPECT_EQ(result1, 6);
  // Повторный вызов getResult должен вернуть тот же результат
  int result2 = scheduler.getResult<int>(id);
  EXPECT_EQ(result2, 6);
}

TEST(TTaskSchedulerTest, AddTaskWithMultipleDependencies) {
  TTaskScheduler scheduler;
  TaskID id1 = scheduler.add(simpleFunc, 2);
  TaskID id2 = scheduler.add(simpleFunc, 3);
  TaskID id3 = scheduler.add(multiplyFunc, scheduler.getFutureResult<int>(id1), scheduler.getFutureResult<int>(id2));
  scheduler.executeAll();
  int result = scheduler.getResult<int>(id3);
  EXPECT_EQ(result, 12);
}


TEST(TTaskSchedulerTest, ComplexTaskChainExecution) {
  TTaskScheduler scheduler;
  TaskID id1 = scheduler.add(simpleFunc, 1);
  TaskID id2 = scheduler.add(simpleFunc, 2);
  TaskID id3 = scheduler.add(addFunc, scheduler.getFutureResult<int>(id1), scheduler.getFutureResult<int>(id2));
  TaskID id4 = scheduler.add(multiplyFunc, scheduler.getFutureResult<int>(id3), 2);
  scheduler.executeAll();
  int result1 = scheduler.getResult<int>(id1);
  int result2 = scheduler.getResult<int>(id2);
  int result3 = scheduler.getResult<int>(id3);
  int result4 = scheduler.getResult<int>(id4);
  EXPECT_EQ(result1, 2);
  EXPECT_EQ(result2, 3);
  EXPECT_EQ(result3, 5);
  EXPECT_EQ(result4, 10);
}

TEST(TTaskSchedulerTest, GetResultBeforeExecuteAll) {
  TTaskScheduler scheduler;
  TaskID id = scheduler.add(simpleFunc, 5);
  try {
    int result = scheduler.getResult<int>(id);
    EXPECT_EQ(result, 6);
  } catch (...) {
    FAIL() << "Expected no exception";
  }
}

TEST(TTaskSchedulerTest, AddTaskWithMixedDependencies) {
  TTaskScheduler scheduler;
  TaskID id1 = scheduler.add(simpleFunc, 2);
  auto future1 = scheduler.getFutureResult<int>(id1);
  TaskID id2 = scheduler.add(multiplyFunc, 4, future1);
  scheduler.executeAll();
  int result = scheduler.getResult<int>(id2);
  EXPECT_EQ(result, 12);
}