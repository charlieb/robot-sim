P = robot_sim
CC = c99
CFLAGS += -g -Wall -O0 `pkg-config --cflags gtk+-2.0` 
LDLIBS += -lm `pkg-config --libs gtk+-2.0` 

$(P): $(OBJECTS)


