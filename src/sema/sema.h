#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

extern SemaphoreHandle_t sema;
extern QueueHandle_t queue;

extern void taskA();
extern void taskB();
extern void taskC();
