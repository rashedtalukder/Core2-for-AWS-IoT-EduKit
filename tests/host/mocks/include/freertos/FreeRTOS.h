#pragma once
#include <stddef.h>
#include <stdint.h>

typedef unsigned int TickType_t;
typedef int BaseType_t;
typedef void *TaskHandle_t;
typedef struct { unsigned int depth; } StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(milliseconds) ((milliseconds) / 10)