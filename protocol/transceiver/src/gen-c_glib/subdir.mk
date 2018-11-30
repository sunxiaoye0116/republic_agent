################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/gen-c_glib/edu_rice_bold_service_bcd_service.c \
../src/gen-c_glib/edu_rice_bold_service_broadcast_types.c 

OBJS += \
./src/gen-c_glib/edu_rice_bold_service_bcd_service.o \
./src/gen-c_glib/edu_rice_bold_service_broadcast_types.o 

C_DEPS += \
./src/gen-c_glib/edu_rice_bold_service_bcd_service.d \
./src/gen-c_glib/edu_rice_bold_service_broadcast_types.d 


# Each subdirectory must supply rules for building sources it contributes
src/gen-c_glib/%.o: ../src/gen-c_glib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Solaris C Compiler'
	gcc -I/usr/local/include/apr-1 -I/usr/local/include -I/usr/lib/zlog/include -I/usr/ext/apr/include/apr-1 -I/usr/ext/apr-util/include/apr-1 -I/usr/lib/apr/include/apr-1 -I/usr/lib/apr-util/include/apr-1 -I/usr/include/glib-2.0/ -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -Igen-c_glib -O0 -g3 -pedantic -Wall -Wextra -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


