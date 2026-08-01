#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// mutex_guard.h
//
// Minimal RAII wrapper around a FreeRTOS mutex (a SemaphoreHandle_t
// created via xSemaphoreCreateMutex()). Takes the mutex on construction,
// blocking until it's free, and releases it on destruction - including
// on every early `return` inside a guarded function, which is the whole
// point: hand-rolled xSemaphoreTake()/xSemaphoreGive() pairs are one
// missed early return away from a permanent deadlock, and both
// obd_response_assembler.cpp and obd_log.cpp have several.
//
// Deliberately NOT portMUX_TYPE / portENTER_CRITICAL(). Those are
// spinlocks intended for data also touched from an ISR: they disable
// interrupts for the duration, so the code inside must be a handful of
// instructions with no blocking calls and no heap allocation. The
// contention this project actually has is between two ordinary FreeRTOS
// tasks - Arduino's loop() task, and NimBLE's own host-stack task, which
// invokes GATT notification callbacks (see ble_gateway.cpp's
// notifyCallback()) from its own task context, not an ISR. The sections
// this guards do String concatenation and struct copies, which allocate
// - unsafe under a spinlock-style critical section, and unnecessary
// besides, since a blocking mutex is the correct primitive for
// task-vs-task contention in the first place.
class MutexGuard {
 public:
  explicit MutexGuard(SemaphoreHandle_t mutex) : mutex_(mutex) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
  }
  ~MutexGuard() { xSemaphoreGive(mutex_); }

  // Non-copyable, non-movable - a guard's whole job is tied to the
  // lifetime of one stack frame's hold on the mutex. Copying or moving
  // it would make "who releases this and when" ambiguous, which is
  // exactly the class of bug this exists to rule out.
  MutexGuard(const MutexGuard &) = delete;
  MutexGuard &operator=(const MutexGuard &) = delete;

 private:
  SemaphoreHandle_t mutex_;
};
