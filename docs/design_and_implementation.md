[Click here](../README.md) to view the README.

## Design and implementation

The design of this application is minimalistic to get started with code examples on PSOC&trade; Edge MCU devices. All PSOC&trade; Edge E84 MCU applications have a dual-CPU three-project structure to develop code for the CM33 and CM55 cores. The CM33 core has two separate projects for the secure processing environment (SPE) and non-secure processing environment (NSPE). A project folder consists of various subfolders, each denoting a specific aspect of the project. The three project folders are as follows:

**Table 1. Application projects**

Project | Description
--------|------------------------
*proj_cm33_s* | Project for CM33 secure processing environment (SPE)
*proj_cm33_ns* | Project for CM33 non-secure processing environment (NSPE)
*proj_cm55* | CM55 project

<br>

In this code example, at device reset, the secure boot process starts from the ROM boot with the secure enclave (SE) as the root of trust (RoT). From the secure enclave, the boot flow is passed on to the system CPU subsystem where the secure CM33 application starts. After all necessary secure configurations, the flow is passed on to the non-secure CM33 application. Resource initialization for this example is performed by this CM33 non-secure project. It configures the system clocks, pins, clock to peripheral connections, and other platform resources. It then enables the CM55 core using the `Cy_SysEnableCM55()` function and the CM55 core is subsequently put to DeepSleep mode.

The firmware routine in CM33 CPU non-secure application code demonstrates how to enter the DeepSleep and Hibernate modes, and use RTC to generate an RTC alarm to wake up the MCU from DeepSleep and Hibernate modes. The main loop checks which user button is pressed (SW2 or SW4).

- If the **SW2** button is pressed, it sets the RTC alarm, and then put it into DeepSleep mode; the RTC alarm interrupt generates after 10 seconds, then print the DeepSleep wakeup information by UART

- If the **SW4** button is pressed, it sets the RTC alarm, the source to wake up the device from the Hibernate mode is configured as RTC alarm, and then the system goes into the Hibernate mode; the RTC alarm generates after 10 seconds leading to MCU reset. The main checks if the reason for reset is Hibernate wakeup with the help of the `Cy_SysLib_GetResetReason()` function. If the reason for the reset is Hibernate wakeup, then prints the Hibernate wakeup information by UART.

**Figure 1. RTC periodic wakeup flowchart**

![](images/power-diagram.png)