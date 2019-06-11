CC = gcc 
CFLAGS = -Wall -Wconversion
#CFLAGS += -O2
CFLAGS += -ggdb
#CFLAGS += -DDEBUG

KATCPDIR=../katcp_devel/katcp

LIBS = -L $(KATCPDIR) -lkatcp 
INC = -I $(KATCPDIR)

LDFLAGS = 

RM = rm -f 

#SRC = small_server.c array_handling.c html_handling.c http_handling.c sensor_parsing.c
SRC = test_program.c sensor.c device.c engine.c vdevice.c host.c team.c array.c tokenise.c
EXE = test_program
OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(EXE)

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

clean: 
	$(RM) core $(EXE) $(OBJ)
