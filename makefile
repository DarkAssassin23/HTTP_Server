TARGET = server
LIBS = -lpthread
CC = gcc
CFLAGS = -g -Wall -pedantic
FLAGS = -DVERBOSE

OBJDIR = obj
INCLUDES = -I headers/

.PHONY: default all clean release

default: $(TARGET)
all: default

release: CFLAGS = -Os -s
release: FLAGS = 
release: $(TARGET)

OBJECTS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(wildcard src/*.c))
HEADERS = $(wildcard headers/*.h)

$(OBJDIR)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(INCLUDES) $(FLAGS) -c $< -o $@
	@echo "  CC      "$@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	@$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) -o $@
	@echo "Created -> "$@

clean:
	$(RM) -r $(OBJDIR) $(TARGET)
