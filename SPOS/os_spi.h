/*
 * os_spi.h
 *
 * Created: 14.06.2024 15:31:58
 *  Author: Ich
 */ 

#include "util.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>
#include "os_scheduler.h"


#ifndef OS_SPI_H_
#define OS_SPI_H_

void os_spi_init();

uint8_t os_spi_send(uint8_t data);

uint8_t os_spi_receive();


#endif /* OS_SPI_H_ */