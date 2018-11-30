################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/checksum.c \
../src/dll.c \
../src/global.c \
../src/hero.c \
../src/main.c \
../src/msg.c \
../src/netmap_util.c \
../src/sys_util.c \
../src/thread_data.c \
../src/thread_recv.c \
../src/thread_send.c \
../src/thread_service.c 

OBJS += \
./src/checksum.o \
./src/dll.o \
./src/global.o \
./src/hero.o \
./src/main.o \
./src/msg.o \
./src/netmap_util.o \
./src/sys_util.o \
./src/thread_data.o \
./src/thread_recv.o \
./src/thread_send.o \
./src/thread_service.o 

C_DEPS += \
./src/checksum.d \
./src/dll.d \
./src/global.d \
./src/hero.d \
./src/main.d \
./src/msg.d \
./src/netmap_util.d \
./src/sys_util.d \
./src/thread_data.d \
./src/thread_recv.d \
./src/thread_send.d \
./src/thread_service.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Solaris C Compiler'
	gcc -I/usr/local/include/apr-1 -I/usr/local/include -I/usr/lib/zlog/include -I/usr/ext/apr/include/apr-1 -I/usr/ext/apr-util/include/apr-1 -I/usr/lib/apr/include/apr-1 -I/usr/lib/apr-util/include/apr-1 -I/usr/include/glib-2.0/ -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -Igen-c_glib -O0 -g3 -pedantic -Wall -Wextra -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


