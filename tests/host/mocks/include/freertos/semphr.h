#pragma once
#include "FreeRTOS.h"

SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *storage);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, TickType_t timeout);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t mutex);
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t mutex);
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage);
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex);