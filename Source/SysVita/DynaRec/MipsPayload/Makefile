TARGET = payload
OBJS   = start.o main.o libc.o
LIBS   = -lkermit

CC    = psp-gcc
CXX   = psp-g++
STRIP = psp-strip

CFLAGS   = -O2 -G0 -Wall -fno-pic
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)
LDFLAGS  = -nostartfiles -nostdlib -T linker.x $(LIBS)
DEPS     = $(OBJS:.o=.d)

all: $(TARGET).bin

%.bin: %.tmp
	pcbctool -e $^ $@

$(TARGET).tmp: $(TARGET).elf
	$(STRIP) -s -O binary $^ -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

.PHONY: clean

clean:
	@rm -f $(TARGET).enc $(TARGET).bin $(TARGET).elf $(OBJS) $(DEPS)

-include $(DEPS)
