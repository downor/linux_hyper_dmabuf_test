TARGET=hyper_dmabuf_app
DBG_FLAGS= -g
CC ?= gcc

MAKE += --no-print-directory

INCLUDES=-I$(PKG_CONFIG_SYSROOT_DIR)/usr/include -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/libdrm -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/drm

LIBS=-L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -ldrm -ldrm_intel

CFLAGS=-Wall $(DBG_FLAGS) -DMESA_EGL_NO_X11_HEADERS

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADER = $(wildcard *.h)

%.o: %.c
	$(CC) $(DBG_FLAGS) -c $(CFLAGS) $^ $(INCLUDES) -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	echo -e "Cleaning..."
	rm -f $(TARGET) *.o
