CC=gcc
ALL_CFLAGS = -fPIC -O3 $(CFLAGS)
#INSTALL_PATH=$(LADSPA_PATH)


PLUGINS=cmeamp.so cmepan.so cmebal.so cmeter.so


all: $(PLUGINS)

install: $(PLUGINS)
	install $(PLUGINS) $(LADSPA_PATH)

.PHONY: clean
clean:
	rm -f *.so *.o

# Basic mono and stereo gain (+/- 120 dB)

cmeamp.o: cmeamp.c
	$(CC) -Wall -Werror $(ALL_CFLAGS) -o $@ -c $<

cmeamp.so: cmeamp.o
	ld -o $@ $< -shared



# Pan (mono in, stereo out) plugin

cmepan.so: cmepan.o
	ld -o $@ $< -shared

cmepan.o: cmepan.c
	$(CC) -std=c99 $(ALL_CFLAGS) -o $@ -c $<



# Balance (stereo in, stereo out) plugin

cmebal.so: cmebal.o
	ld -o $@ $< -shared

cmebal.o: cmebal.c
	$(CC) -std=c99 $(ALL_CFLAGS) -o $@ -c $<


# Level meter plugin

cmeter.so: cmeter.o
	ld -o $@ $< -shared

cmeter.o: cmeter.c
	$(CC) $(ALL_CFLAGS) -o $@ -c $<

