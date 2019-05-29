#include "stm32f103xb.h"
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "sema.h"

int main(void)
{
	sema = xSemaphoreCreateBinary();
	queue = xQueueCreate(16, sizeof(uint8_t));

	xTaskCreate(taskA, "a", 128, NULL, configMAX_PRIORITIES - 1, NULL);
	xTaskCreate(taskB, "b", 128, NULL, configMAX_PRIORITIES - 1, NULL);
	xTaskCreate(taskC, "c", 128, NULL, configMAX_PRIORITIES - 1, NULL);

	xSemaphoreGive(sema);

	vTaskStartScheduler();

	return 0;
}

