/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for RTC alarm periodic wakeup Example
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/******************************************************************************
* Header files
******************************************************************************/
#include "retarget_io_init.h"
#include "time.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* Initial Time and Date definitions */
#define RTC_INITIAL_DATE_SEC        (0U)
#define RTC_INITIAL_DATE_MIN        (17U)
#define RTC_INITIAL_DATE_HOUR       (16U)
#define RTC_INITIAL_DATE_DAY        (28U)
#define RTC_INITIAL_DATE_MONTH      (2U)
#define RTC_INITIAL_DATE_YEAR       (2024U)
#define RTC_INTERRUPT_PRIORITY      (3U)
#define STRING_BUFFER_SIZE          (80U)
#define RTC_CENTURY                 (2000U)
#define TM_YEAR_BASE                (1900U)

/* Macro for Alarm set by seconds */
#define USE_SECONDS_FOR_ALARM       (10U)

#define RTC_ACCESS_RETRY_COUNT      (5U)
#define RTC_RETRY_DELAY_MS          (5U)

/* The timeout value in microsecond used to wait for core to be booted */
#define CM55_BOOT_WAIT_TIME_USEC    (10U)

/* App boot address for CM55 project */
#define CM55_APP_BOOT_ADDR          (CYMEM_CM33_0_m55_nvm_START + \
                                        CYBSP_MCUBOOT_HEADER_SIZE)

#define BTN_IRQ_PRIORITY            (7U)
#define RTC_IRQ_PRIORITY            (3U)
#define LOOP_DELAY_MS               (50U)

/*******************************************************************************
* Global Variables
*******************************************************************************/
volatile bool gpio_intr_flag = false;

typedef enum
{
    SWITCH_NO_EVENT     = 0U,
    SWITCH_DEEPSLEEP    = 1U,
    SWITCH_HIBERNATE    = 2U,
} en_switch_event_t;

volatile en_switch_event_t button_status;


/*******************************************************************************
* Function Name: debug_printf
********************************************************************************
* Summary:
* This function prints out the current date time and user string.
*
* Parameters:
*  str      Points to the user print string.
*
* Return:
*  void
*
*******************************************************************************/
void debug_printf(const char *str)
{
    uint32_t year;
    struct tm date_time;
    char buffer[STRING_BUFFER_SIZE];
    cy_stc_rtc_config_t curr_date_time;

    /* Get the current date and time */
    Cy_RTC_GetDateAndTime(&curr_date_time);

    year = RTC_CENTURY + curr_date_time.year;

    /* The number of days that precede each month of the year, 
    not including Feb 29 */
    static const uint16_t CUMULATIVE_DAYS[] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    date_time.tm_sec  = (int32_t)curr_date_time.sec;
    date_time.tm_min  = (int32_t)curr_date_time.min;
    date_time.tm_hour = (int32_t)curr_date_time.hour;
    date_time.tm_mday = (int32_t)curr_date_time.date;
    date_time.tm_mon  = (int32_t)(curr_date_time.month - 1u);
    date_time.tm_year = (int32_t)(year - TM_YEAR_BASE);

    date_time.tm_wday = (int32_t)(curr_date_time.dayOfWeek - 1u);

    date_time.tm_yday = (int32_t)CUMULATIVE_DAYS[date_time.tm_mon] +
                        (int32_t)curr_date_time.date - 1 +
                        (((int32_t)(curr_date_time.month) >= 3 &&
                        (int32_t)(Cy_RTC_IsLeapYear((uint32_t)year) ? 1u : 0u)));

    date_time.tm_isdst = -1;

    strftime(buffer, sizeof(buffer), "%X %F", &date_time);
    printf("%s: %s", buffer, str);
    memset(buffer, '\0', sizeof(buffer));
}


/*******************************************************************************
* Function Name: set_rtc_alarm_date_time
********************************************************************************
* Summary:
*  This functions sets the RTC alarm date and time.
*
* Parameter:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void set_rtc_alarm_date_time(void)
{
    cy_stc_rtc_config_t date_time;
    cy_en_rtc_status_t rtc_status;
    uint32_t rtc_access_retry = RTC_ACCESS_RETRY_COUNT;

    /* Print the RTC alarm time by UART */
    debug_printf("RTC alarm will be generated after 10 seconds\r\n");

    Cy_RTC_GetDateAndTime(&date_time);

    /* Set the RTC alarm for the specified number of seconds in the future */
    if (date_time.sec + USE_SECONDS_FOR_ALARM > 59) 
    {
        date_time.sec = ((date_time.sec + USE_SECONDS_FOR_ALARM) - 59);
        date_time.min++;
    }
    else 
    {
        date_time.sec += USE_SECONDS_FOR_ALARM;
    }

    /* Set alarm */
    do {
        rtc_status = Cy_RTC_SetAlarmDateAndTimeDirect(
            date_time.sec,
            date_time.min,
            date_time.hour,
            date_time.date,
            date_time.month,
            CY_RTC_ALARM_1
        );

        rtc_access_retry--;
        Cy_SysLib_Delay(RTC_RETRY_DELAY_MS);
    } while ((rtc_status != CY_RTC_SUCCESS) && (rtc_access_retry != 0));
}

/*******************************************************************************
* Function Name: gpio_isr_handler
********************************************************************************
* Summary:
*  Interrupt serive routine for CYBSP_USER_BTN_PORT and CYBSP_USER_BTN2_PORT.
*  This function checks which user button was presssed and sets the
*  button_status flag.
*
* Parameter:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void gpio_isr_handler(void)
{
    /* Check if USER_BTN1 was pressed */
    if(1UL == Cy_GPIO_GetInterruptStatus(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_PIN))
    {
        button_status = SWITCH_DEEPSLEEP;
        /* Clear the USER_BTN1 interrupt */
        Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_PIN);
    }

    /* Check if USER_BTN2 was pressed */
    if(1UL == Cy_GPIO_GetInterruptStatus(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN))
    {
        button_status = SWITCH_HIBERNATE;
        /* Clear the USER_BTN2 interrupt */
        Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
    }
}

/*******************************************************************************
* Function Name: rtc_isr
********************************************************************************
* Summary:
*  This function calls the RTC interrupt handler function
*
* Parameter:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void rtc_isr(void)
{
    Cy_RTC_Interrupt(NULL, false);
}


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for the CM33 CPU NSPE.
*    1. Initialize the UART and RTC blocks.
*    2. Check the reset reason, if it is wakeup from Hibernate power mode, then
*       set RTC initial time and date.
*    Do Forever loop:
*    3. Check if User button was pressed.
*    4. If SW2 is pressed, set the RTC alarm and then go to DeepSleep mode.
*    5. If SW4 is pressed, set the RTC alarm and then go to Hibernate mode.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    cy_en_rtc_status_t rtc_status;
    cy_en_sysint_status_t int_status;

    cy_stc_sysint_t gpio_int_config =
    {
        .intrSrc = ioss_interrupts_gpio_8_IRQn,
        .intrPriority = BTN_IRQ_PRIORITY,
    };

    const cy_stc_sysint_t rtc_intr_config =
    {
        .intrSrc = srss_interrupt_rtc_IRQn,
        .intrPriority  = RTC_IRQ_PRIORITY,
    };

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io middleware */
    init_retarget_io();

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("*************************************************************\r\n");
    printf("PSOC Edge MCU: RTC periodic wakeup\r\n");
    printf("*************************************************************\r\n");
    printf("Press 'SW2' key to enter DeepSleep mode.\r\n\r\n");
    printf("Press 'SW4' key to enter Hibernate mode.\r\n\r\n");

    int_status = Cy_SysInt_Init(&gpio_int_config, gpio_isr_handler);
    if(CY_SYSINT_SUCCESS != int_status)
    {
        printf("Interrupt initialization failed failed\r\n");
    }

    NVIC_EnableIRQ(gpio_int_config.intrSrc);

    /* Enable CM55. */
    /* CM55_APP_BOOT_ADDR must be updated if CM55 memory layout is changed.*/
    Cy_SysEnableCM55(MXCM55, CM55_APP_BOOT_ADDR, CM55_BOOT_WAIT_TIME_USEC);

    /* System Domain Idle Power Mode Configuration */
    Cy_SysPm_SetDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP);

    /* SoCMEM Idle Power Mode Configuration */
    Cy_SysPm_SetSOCMEMDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP);

    /* Check the reset reason */
    if(CY_SYSLIB_RESET_HIB_WAKEUP ==
        (Cy_SysLib_GetResetReason() & CY_SYSLIB_RESET_HIB_WAKEUP))
    {
        /* The reset has occurred on a wakeup from Hibernate power mode */
        debug_printf("Wakeup from the Hibernate mode\r\n");

        /* Clear reset reason bit */
        Cy_SysLib_ClearResetReason();
    }
    else
    {
        /* Initialize RTC */
        rtc_status = Cy_RTC_Init(&CYBSP_RTC_config);

        if(CY_RTC_SUCCESS != rtc_status)
        {
            printf("RTC initialization failed\r\n");
        }
    }

    /* Print the current date and time by UART */
    debug_printf("Current date and time.\r\n");

    Cy_RTC_SetInterruptMask(CY_RTC_INTR_ALARM1);

    /* Configure RTC interrupt ISR */
    Cy_SysInt_Init(&rtc_intr_config, rtc_isr);

    NVIC_ClearPendingIRQ(rtc_intr_config.intrSrc);

    NVIC_EnableIRQ(rtc_intr_config.intrSrc);

    button_status = SWITCH_NO_EVENT;

    for (;;)
    {
        if(button_status != SWITCH_NO_EVENT)
        {
            switch (button_status)
            {
                case SWITCH_DEEPSLEEP:

                    /* Button Debounce delay */
                    Cy_SysLib_Delay(100);

                    debug_printf("Go to DeepSleep mode\r\n");

                    /* Set the RTC generate alarm after 10 seconds */
                    set_rtc_alarm_date_time();

                    button_status = SWITCH_NO_EVENT;

                    /* Wait for UART traffic to stop */
                    while(!(Cy_SCB_UART_IsTxComplete(CYBSP_DEBUG_UART_HW))) {};

                    /* Go to deep sleep */
                    Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);

                    debug_printf("Wakeup from DeepSleep mode\r\n");
                    break;

                case SWITCH_HIBERNATE:

                    /* Button Debounce delay */
                    Cy_SysLib_Delay(100);
                    
                    debug_printf("Go to Hibernate mode\r\n");

                    /* Set the RTC generate alarm after 10 seconds */
                    set_rtc_alarm_date_time();

                    button_status = SWITCH_NO_EVENT;

                    /* Wait for UART traffic to stop */
                    while(!(Cy_SCB_UART_IsTxComplete(CYBSP_DEBUG_UART_HW))) {};

                    /* Scenario: There is a need to configure all desired system Hibernate wake up sources */
                    Cy_SysPm_SetHibernateWakeupSource(CY_SYSPM_HIBERNATE_RTC_ALARM);

                    /* Go to hibernate */
                    Cy_SysPm_SystemEnterHibernate();
                    break;

                default:
                    break;
            }
        }
        Cy_SysLib_Delay(LOOP_DELAY_MS);
    }
}

/* [] END OF FILE */