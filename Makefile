all: final.hex

final.hex: final.ihx
	packihx final.ihx > final.hex

final.ihx: final.c
	sdcc final.c

OBJS = *.asm *.hex *.ihx *.lk *.lst *.map *.rel *.rst *.sym *.mem
clean:
	del $(OBJS)