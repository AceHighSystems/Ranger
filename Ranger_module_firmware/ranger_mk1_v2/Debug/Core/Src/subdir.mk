################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/DRV8462.c \
../Core/Src/FDC2214.c \
../Core/Src/INA229.c \
C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ace_protocol.c \
../Core/Src/main.c \
../Core/Src/ranger_app.c \
C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ranger_can.c \
../Core/Src/ranger_param.c \
../Core/Src/stm32g4xx_hal_msp.c \
../Core/Src/stm32g4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32g4xx.c 

OBJS += \
./Core/Src/DRV8462.o \
./Core/Src/FDC2214.o \
./Core/Src/INA229.o \
./Core/Src/ace_protocol.o \
./Core/Src/main.o \
./Core/Src/ranger_app.o \
./Core/Src/ranger_can.o \
./Core/Src/ranger_param.o \
./Core/Src/stm32g4xx_hal_msp.o \
./Core/Src/stm32g4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32g4xx.o 

C_DEPS += \
./Core/Src/DRV8462.d \
./Core/Src/FDC2214.d \
./Core/Src/INA229.d \
./Core/Src/ace_protocol.d \
./Core/Src/main.d \
./Core/Src/ranger_app.d \
./Core/Src/ranger_can.d \
./Core/Src/ranger_param.d \
./Core/Src/stm32g4xx_hal_msp.d \
./Core/Src/stm32g4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32g4xx.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I/Users/tor/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Inc -IC:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src -IC:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Inc -I/Users/tor/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/ace_protocol.o: C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ace_protocol.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I/Users/tor/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Inc -IC:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src -IC:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Inc -I/Users/tor/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/ranger_can.o: C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ranger_can.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I/Users/tor/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Inc -IC:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src -IC:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Inc -I/Users/tor/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/DRV8462.cyclo ./Core/Src/DRV8462.d ./Core/Src/DRV8462.o ./Core/Src/DRV8462.su ./Core/Src/FDC2214.cyclo ./Core/Src/FDC2214.d ./Core/Src/FDC2214.o ./Core/Src/FDC2214.su ./Core/Src/INA229.cyclo ./Core/Src/INA229.d ./Core/Src/INA229.o ./Core/Src/INA229.su ./Core/Src/ace_protocol.cyclo ./Core/Src/ace_protocol.d ./Core/Src/ace_protocol.o ./Core/Src/ace_protocol.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/ranger_app.cyclo ./Core/Src/ranger_app.d ./Core/Src/ranger_app.o ./Core/Src/ranger_app.su ./Core/Src/ranger_can.cyclo ./Core/Src/ranger_can.d ./Core/Src/ranger_can.o ./Core/Src/ranger_can.su ./Core/Src/ranger_param.cyclo ./Core/Src/ranger_param.d ./Core/Src/ranger_param.o ./Core/Src/ranger_param.su ./Core/Src/stm32g4xx_hal_msp.cyclo ./Core/Src/stm32g4xx_hal_msp.d ./Core/Src/stm32g4xx_hal_msp.o ./Core/Src/stm32g4xx_hal_msp.su ./Core/Src/stm32g4xx_it.cyclo ./Core/Src/stm32g4xx_it.d ./Core/Src/stm32g4xx_it.o ./Core/Src/stm32g4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32g4xx.cyclo ./Core/Src/system_stm32g4xx.d ./Core/Src/system_stm32g4xx.o ./Core/Src/system_stm32g4xx.su

.PHONY: clean-Core-2f-Src

