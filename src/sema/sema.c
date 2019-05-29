#include "sema.h"
#include "FreeRTOS.h"
#include "task.h"

SemaphoreHandle_t sema;
QueueHandle_t queue;

void taskA()
{
	uint8_t a = 'a';
	while (1)
	{
		if (xSemaphoreTake(sema, portMAX_DELAY) == pdTRUE)
		{
			xQueueSend(queue, &a, pdMS_TO_TICKS(1000));
			xSemaphoreGive(sema);
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void taskB()
{
	uint8_t b = 'b';
	while (1)
	{
		if (xSemaphoreTake(sema, portMAX_DELAY) == pdTRUE)
		{
			xQueueSend(queue, &b, pdMS_TO_TICKS(1000));
			xSemaphoreGive(sema);
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void taskC()
{
	uint8_t c;
	while (1)
	{
		if (xQueuePeek(queue, &c, pdMS_TO_TICKS(1000)) == pdTRUE)
		{
			if (xSemaphoreTake(sema, pdMS_TO_TICKS(1000)) == pdTRUE)
			{
				xQueueReceive(queue, &c, pdMS_TO_TICKS(1000));
				volatile uint8_t d = c;
				++d;
				xSemaphoreGive(sema);
			}
		}
	}
}
