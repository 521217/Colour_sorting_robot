#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "octo.h"
#include "FreeRTOS.h"
#include "task.h"

position_t presentPositions[48];
position_t goalPositions[48];
uint8_t pings[48];

QueueHandle_t xQueue;
QueueHandle_t uartSignalQueue;
QueueHandle_t uartResultQueue;
QueueHandle_t packetQueue;

void led_task()
{
	while(1)
	{
		uint8_t speed;
		xQueueReceive(xQueue, &speed, portMAX_DELAY);
		GPIOC_SR = (1 << 29);
		vTaskDelay(pdMS_TO_TICKS(250 * speed));
		GPIOC_SR = (1 << 13);
		vTaskDelay(pdMS_TO_TICKS(250 * speed));
	}
}

void test_task()
{
	uint8_t n = 0;
	uint8_t fast = 1;
	uint8_t slow = 4;

	while (1)
	{
		++n;
		if (n <= 3)
		{
			xQueueSend(xQueue, &slow, portMAX_DELAY);
		}
		else if (n <= 6)
		{
			xQueueSend(xQueue, &fast, portMAX_DELAY);
		}
		else
		{
			n = 0;
		}
	}
}

void arm_controller_task()
{
	//These are the wrong numbers.
	//They should be in units instead of degrees.
	//150 / 0.29 = 517.
	goalPositions[0].x = 150;
	goalPositions[1].x = 195;
	goalPositions[2].x = 60;
	goalPositions[3].x = 60;
	goalPositions[4].x = 150;
	goalPositions[5].x = 150;

	goalPositions[6].x = 150;
	goalPositions[7].x = 195;
	goalPositions[8].x = 60;
	goalPositions[9].x = 60;
	goalPositions[10].x = 150;
	goalPositions[11].x = 150;

	goalPositions[42].x = 150;
	goalPositions[43].x = 195;
	goalPositions[44].x = 60;
	goalPositions[45].x = 60;
	goalPositions[46].x = 150;
	goalPositions[47].x = 150;

	instruction_t ins1 = {0, 1, 240, 60, 0, "t1", "t2"};
	instruction_t ins2 = {0, 2, 240, 150, 0, "t2", "f2"};
	instruction_t ins3 = {0, 8, 240, 60, 0, "t8", "t1"};
	instruction_t *inss[] = {&ins1, &ins2, &ins3};
	uint8_t inss_length = sizeof(inss) / sizeof(instruction_t *);

	while (1)
	{
		uint8_t flags = 0;
		for (int i = 0; i < inss_length; ++i)
		{
			instruction_t *ins = inss[i];
			if (ins->flag)
			{
				++flags;
				continue;
			}
			else
			{
				uint8_t allowed = 1;
				for (int j = 0; j < i; ++j)
				{
					instruction_t *prev_ins = inss[j];
					if (prev_ins->flag)
					{
						continue;
					}
					else
					{
						if (strcmp(ins->from, prev_ins->from) == 0
							|| strcmp(ins->from, prev_ins->to) == 0
							|| strcmp(ins->to, prev_ins->from) == 0
							|| strcmp(ins->to, prev_ins->to) == 0)
						{
							allowed = 0;
							break;
						}
					}
				}

				if (!allowed)
				{
					continue;
				}

				uint8_t stateChangeComplete = 1;
				for (int j = 0; j < 6; ++j)
				{
					uint8_t index = (ins->arm - 1) * 6 + j;
					if (abs(presentPositions[index].x - goalPositions[index].x) > 5)
					{
						//stateChangeComplete = 0;
					}
				}

				if (!stateChangeComplete)
				{
					continue;
				}

				switch (ins->state)
				{
					case 0: //rotate
						rotate(ins->arm, ins->r1);
						break;
					case 1: //extend
						stretch(ins->arm, 105, 105, 105);
						break;
					case 2: //close
						claw(ins->arm, 130, 170);
						break;
					case 3: //lift
						wrist(ins->arm, 60);
						break;
					case 4: //retract
						stretch(ins->arm, 195, 60, 60);
						break;
					case 5: //rotate
						rotate(ins->arm, ins->r2);
						break;
					case 6: //extend
						stretch(ins->arm, 105, 105, 60);
						break;
					case 7: //put
						wrist(ins->arm, 105);
						break;
					case 8: //open
						claw(ins->arm, 150, 150);
						break;
					case 9: //retract
						stretch(ins->arm, 195, 60, 60);
						break;
					case 10: //rotate
						rotate(ins->arm, 150);
						break;
					case 11:
						ins->flag = 1;
						continue;
					default: //Something went wrong.
						continue;
				}

				++ins->state;
			}
		}

		if (flags >= inss_length)
		{
			break;
		}
	}
}

void uart_controller_task()
{
	ax_packet_t packet;
	uint8_t header = 0xFF;
	uint8_t length;
	uint8_t type;
	uint8_t crc;
	uint8_t dummy;
	uint8_t bytes;
	uint8_t byte;

	while (1)
	{
		xQueueReceive(packetQueue, &packet, portMAX_DELAY);

		length = packet.params_length + 2;
		type = (uint8_t)packet.type;
		crc = ax_crc(packet);
		bytes = 6 + packet.params_length;
		byte = 0;

		for (int i = 0; i < bytes; ++i)
		{
			if (i <= 1)
			{
				byte = header;
			}
			else if (i == 2)
			{
				byte = packet.id;
			}
			else if (i == 3)
			{
				byte = length;
			}
			else if (i == 4)
			{
				byte = type;
			}
			else if (i >= 5 && i <= bytes - 2)
			{
				byte = packet.params[i - 5];
			}
			else if (i == bytes - 1)
			{
				byte = crc;
			}
		}

		_USART_DR = byte;

		//Get signal from isr when byte has been transmitted.
		xQueueReceive(uartSignalQueue, &dummy, portMAX_DELAY);
	}

	uint8_t result;
	uint8_t index = idToIndex(packet.id);

	switch (packet.type)
	{
		case PING:
			xQueueReceive(uartResultQueue, &result, portMAX_DELAY);
			pings[index] = result;
			break;
		case READ:
			//Care for endianness.
			for (int i = 0; i < packet.params[1]; ++i)
			{
				xQueueReceive(uartResultQueue, &result, portMAX_DELAY);
				presentPositions[index].xa[i] = result;
			}
			break;
		default:
			//Invalid type.
			break;
	}
}

uint8_t ax_crc(ax_packet_t packet) {
	uint8_t crc = packet.id + (packet.params_length + 2) + (uint8_t)packet.type;
	for (int i = 0; i < packet.params_length; ++i) {
		crc += packet.params[i];
	}
	return ~crc;
}

uint8_t idToIndex(uint8_t id) {
	uint8_t motor = id % 10;
	uint8_t arm = (id - motor) / 10;
	return (arm - 1) * 6 + (motor - 1);
}

uint8_t indexToId(uint8_t index) {
	uint8_t motor = (index % 6) + 1;
	uint8_t arm = (index / 6) + 1;
	return (arm * 10 + motor);
}

void rotate(uint8_t arm, uint16_t aDegrees) {
	uint8_t index = (arm - 1) * 6;
	uint8_t aMotor = arm * 10 + 1;
	setSpeed(aMotor, 15);
	setGoalPosition(aMotor, aDegrees);
	goalPositions[index].x = aDegrees / 0.29;
}

void stretch(uint8_t arm, uint16_t bDegrees, uint16_t cDegrees, uint16_t dDegrees) {
	uint8_t index = (arm - 1) * 6;
	uint8_t bMotor = arm * 10 + 2;
	uint8_t cMotor = bMotor + 1;
	uint8_t dMotor = cMotor + 1;
	uint16_t bDegreesDelta = abs(presentPositions[index + 1].x - bDegrees);
	uint16_t cDegreesDelta = abs(presentPositions[index + 2].x - cDegrees);
	uint16_t dDegreesDelta = abs(presentPositions[index + 3].x - dDegrees);
	uint16_t bRpm = (bDegreesDelta / 90.0) * 15;
	uint16_t cRpm = (cDegreesDelta / 90.0) * 15;
	uint16_t dRpm = (dDegreesDelta / 90.0) * 15;
	setSpeed(bMotor, bRpm);
	setSpeed(cMotor, cRpm);
	setSpeed(dMotor, dRpm);
	setGoalPosition(bMotor, bDegrees);
	setGoalPosition(cMotor, cDegrees);
	setGoalPosition(dMotor, dDegrees);
	goalPositions[index+1].x = bDegrees / 0.29;
	goalPositions[index+2].x = cDegrees / 0.29;
	goalPositions[index+3].x = dDegrees / 0.29;
}

void wrist(uint8_t arm, uint16_t dDegrees) {
	uint8_t index = (arm - 1) * 6;
	uint8_t aMotor = arm * 10 + 4;
	setSpeed(aMotor, 15);
	setGoalPosition(aMotor, dDegrees);
	goalPositions[index+3].x = dDegrees / 0.29;
}

void claw(uint8_t arm, uint16_t eDegrees, uint16_t fDegrees) {
	uint8_t index = (arm - 1) * 6;
	uint8_t eMotor = arm * 10 + 5;
	uint8_t fMotor = arm * 10 + 6;
	setSpeed(eMotor, 15);
	setSpeed(fMotor, 15);
	setGoalPosition(eMotor, eDegrees);
	setGoalPosition(fMotor, fDegrees);
	goalPositions[index+4].x = eDegrees / 0.29;
	goalPositions[index+5].x = fDegrees / 0.29;
}


//0--1023, 0.111 rpm per unit
void setSpeed(uint8_t motor, uint16_t rpm) {
	uint16_t units = rpm / 0.111;
	units = units > 1023 ? 1023 : units;
	//TODO: send instruction packet.
}

//0--1023, 0.29 degree per unit
void setGoalPosition(uint8_t motor, uint16_t degrees) {
	uint16_t units = degrees / 0.29;
	units = units > 1023 ? 1023 : units;
	//TODO: send instruction packet.
}