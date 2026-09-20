CROSS_COMPILE = aarch64-linux-gnu-

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS = -ffreestanding \
 -Iinclude \
         -fno-stack-protector \
         -fno-pie \
         -fno-builtin \
         -Wall \
         -Wextra \
         -O0 \
         -g

LDFLAGS = -T linker.ld \
          --nostdlib \
          --static \
          --gc-sections \
          --build-id=none \
          -z max-page-size=0x1000

OBJS = boot/start.o \
       kernel/main.o \
       kernel/uart.o \
       kernel/exception.o \
       kernel/exception-vector.o \
       kernel/gic.o \
       kernel/timer.o \
       kernel/page_alloc.o kernel/fdt.o \
        kernel/task.o \
        kernel/task_switch.o

all: kernel.elf kernel.bin

boot/start.o: boot/start.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/main.o: kernel/main.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/uart.o: kernel/uart.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/exception.o: kernel/exception.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/exception-vector.o: kernel/exception.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/gic.o: kernel/gic.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/timer.o: kernel/timer.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/page_alloc.o: kernel/page_alloc.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) kernel.elf kernel.bin

.PHONY: all clean

kernel/task.o: kernel/task.c include/task.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/task_switch.o: kernel/task_switch.S include/task.h
	$(CC) $(CFLAGS) $(ASFLAGS) -c $< -o $@
