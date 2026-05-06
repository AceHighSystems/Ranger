################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ace_protocol.c \
C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ranger_bootloader.c \
C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ranger_can.c 

OBJS += \
./Core/Src/AceHigh_Src/ace_protocol.o \
./Core/Src/AceHigh_Src/ranger_bootloader.o \
./Core/Src/AceHigh_Src/ranger_can.o 

C_DEPS += \
./Core/Src/AceHigh_Src/ace_protocol.d \
./Core/Src/AceHigh_Src/ranger_bootloader.d \
./Core/Src/AceHigh_Src/ranger_can.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/AceHigh_Src/ace_protocol.o: C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ace_protocol.c Core/Src/AceHigh_Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/AceHigh_Src/ranger_bootloader.o: C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ranger_bootloader.c Core/Src/AceHigh_Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/AceHigh_Src/ranger_can.o: C:/Users/torgj/Documents/Ranger/Ranger_module_firmware/AceHigh_common/AceHigh_Src/ranger_can.c Core/Src/AceHigh_Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-AceHigh_Src

clean-Core-2f-Src-2f-AceHigh_Src:
	-$(RM) ./Core/Src/AceHigh_Src/ace_protocol.cyclo ./Core/Src/AceHigh_Src/ace_protocol.d ./Core/Src/AceHigh_Src/ace_protocol.o ./Core/Src/AceHigh_Src/ace_protocol.su ./Core/Src/AceHigh_Src/ranger_bootloader.cyclo ./Core/Src/AceHigh_Src/ranger_bootloader.d ./Core/Src/AceHigh_Src/ranger_bootloader.o ./Core/Src/AceHigh_Src/ranger_bootloader.su ./Core/Src/AceHigh_Src/ranger_can.cyclo ./Core/Src/AceHigh_Src/ranger_can.d ./Core/Src/AceHigh_Src/ranger_can.o ./Core/Src/AceHigh_Src/ranger_can.su

.PHONY: clean-Core-2f-Src-2f-AceHigh_Src

