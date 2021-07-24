/* Copyright 2016-2021, Laboratorio de Microprocesadores
 * Facultad de Ciencias Exactas y Tecnología
 * Universidad Nacional de Tucuman
 * http://www.microprocesadores.unt.edu.ar/
 * Copyright 2016-2021, Esteban Volentini <evolentini@herrera.unt.edu.ar>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** @file expropiativo.c
 ** @brief Ejemplo de un cambio de contexto expropiativo
 **
 ** Ejemplo de la implementación básica de la ejecución de dos tareas con un planificador muy
 ** elemental tipo round robin. Se imeplementa un cambio de contexto expropiativo basado en
 ** la rutina de servicio de la interrupción del temporizador del sistema. Esta misma
 ** interrupción se utiliza para asignar las cuotas de tiempo de cada proceso.
 **
 **| REV | YYYY.MM.DD | Autor           | Descripción de los cambios                              |
 **|-----|------------|-----------------|---------------------------------------------------------|
 **|   4 | 2021.07.09 | evolentini      | Migracion del ejemplo a firmware CIAA V2                |
 **|   3 | 2017.10.16 | evolentini      | Correción en el formato del archivo                     |
 **|   2 | 2017.09.21 | evolentini      | Cambio para utilizar drivers_bm de UNER                 |
 **|   1 | 2016.06.25 | evolentini      | Version inicial del archivo                             |
 **
 ** @addtogroup modulo
 ** @brief Breve descripción del modulo
 ** @{ */

/* === Inclusiones de cabeceras ================================================================ */
#include <stdint.h>
#include <string.h>

#include "sapi.h"

/* === Definiciones y Macros =================================================================== */

/**
 * @brief Cantidad de tareas implementadas
 */
#define TASK_COUNT 2

/**
 * @brief Cantidad de bytes asignado como pila para cada tarea
 */
#define STACK_SIZE 256

/**
 * @brief Valor del contador para la demora en el parpadeo de la tarea B
 */
#define COUNT_DELAY 3000000

/* === Declaraciones de tipos de datos internos ================================================ */

/**
 * @brief Tipo de datos para recibir la pila de una tarea
 */
typedef uint8_t stack_t[STACK_SIZE];

/* === Declaraciones de funciones internas ===================================================== */

/**
 *  @brief Funcion que configura el temporizador del sistema
 *
 * Esta función configura el temporizador del sistema para generar una interrupcion periodica
 * cada 2ms. Esta resulta la cuota de tiempo de procesador que se asigna a cada tarea.
 */
void ConfigurarInterrupcion(void);

/**
 * @brief Función para generar demoras
 * Función basica que genera una demora para permitir el parpadeo de los leds
 */
void Delay(void);

/**
 * @brief Función que indica un error en el cambio de contexto
 *
 * @remark Esta funcion no debería ejecutarse nunca, solo se accede a la misma si las funciones
 * que implementan las tareas terminan
 */
void Error(void);

/**
 *  @brief Función que implementa la primera tarea del sistema
 */
void TareaA(void);

/**
 * @brief Función que implementa la segunda tarea del sistema
 */
void TareaB(void);

/* === Definiciones de variables internas ====================================================== */

/**
 * @brief Espacio para la pila de las tareas
 */
static stack_t stack[TASK_COUNT];

/**
 * @brief Punteros a contexto de cada tarea y del sistema operativo
 */
static uint32_t context[TASK_COUNT + 1];

/* === Definiciones de variables externas ====================================================== */

/* === Definiciones de funciones internas ====================================================== */

void ConfigurarInterrupcion(void)
{
    __asm__ volatile("cpsid i");

    /* Activate SysTick */
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 5000);

    /* Update priority set by SysTick_Config */
    NVIC_SetPriority(SysTick_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

    __asm__ volatile("cpsie i");
}

__attribute__((naked())) void SysTick_Handler(void)
{
    static int divisor = 0;
    static int activa = TASK_COUNT;

    /* Se salva el contexto de la tarea activa en su correspondiente pila */
    __asm__ volatile("push {r4-r11}");
    __asm__ volatile("str r13, %0" : "=m"(context[activa]));
    __asm__ volatile("ldr r13, %0" : : "m"(context[TASK_COUNT]));

    /* Se determina seleciona la proxima tarea que utilizará el procesador */
    activa = (activa + 1) % TASK_COUNT;
    divisor = (divisor + 1) % 1000;
    if (divisor == 0)
        gpioToggle(LEDB);

    /* Se recupera el contexto de la nueva tarea desde su correspondiente pila */
    __asm__ volatile("str r13, %0" : "=m"(context[TASK_COUNT]));
    __asm__ volatile("ldr r13, %0" : : "m"(context[activa]));
    __asm__ volatile("pop {r4-r11}");

    /*  Se devuelve el uso del procesador a la tarea designada */
    __asm__ volatile("ldr lr,=0xFFFFFFF9");
    __asm__ volatile("bx lr");
}

void Delay(void)
{
    uint32_t i;

    for (i = COUNT_DELAY; i != 0; i--) {
        __asm__ volatile("nop");
    }
}

void Error(void)
{
    gpioWrite(LEDR, true);
    while (1) { }
}

void TareaA(void)
{
    bool valor;
    while (1) {
        valor = !gpioRead(TEC4);
        gpioWrite(LED3, valor);
    }
}

void TareaB(void)
{
    while (1) {
        gpioToggle(LED2);
        Delay();
    }
}

/* === Definiciones de funciones externas ====================================================== */

int main(void)
{
    struct {
        struct {
            uint32_t r4;
            uint32_t r5;
            uint32_t r6;
            uint32_t r7;
            uint32_t r8;
            uint32_t r9;
            uint32_t r10;
            uint32_t r11;
        } context;
        struct {
            uint32_t r0;
            uint32_t r1;
            uint32_t r2;
            uint32_t r3;
            uint32_t ip;
            uint32_t lr;
            uint32_t pc;
            uint32_t xPSR;
        } interrupt;
    } frame_call;

    /* Inicialización de las estructuras de contexto de ambas tareas */
    void* pointer = stack;

    memset(stack, 0, sizeof(frame_call));
    memset(&frame_call, 0, sizeof(frame_call));
    frame_call.interrupt.lr = (uint32_t)Error;
    frame_call.interrupt.xPSR = 0x21000000;

    /* Inicialización del contexto de la tarea A */
    pointer += (STACK_SIZE);
    frame_call.interrupt.pc = (uint32_t)TareaA;
    memcpy(pointer - sizeof(frame_call), &frame_call, sizeof(frame_call));
    context[0] = (uint32_t)(pointer - sizeof(frame_call));

    /* Inicialización del contexto de la tarea B */
    pointer += (STACK_SIZE);
    frame_call.interrupt.pc = (uint32_t)TareaB;
    memcpy(pointer - sizeof(frame_call), &frame_call, sizeof(frame_call));
    context[1] = (uint32_t)(pointer - sizeof(frame_call));

    /* Configuración de los dispositivos de la placa */
    boardConfig();
    ConfigurarInterrupcion();

    /* Espera de la primera interupción para arrancar el sistema */
    while (1) { }

    return 0;
}

/* === Ciere de documentacion ================================================================== */

/** @} Final de la definición del modulo para doxygen */
