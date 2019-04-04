CC = gcc 
CFLAGS = -Wall
#CFLAGS += -O2
CFLAGS += -ggdb
#CFLAGS += -DDEBUG

KATCPDIR=../katcp_devel/katcp

LIBS = -L $(KATCPDIR) -lkatcp 
INC = -I $(KATCPDIR)

LDFLAGS = 

RM = rm -f 

SRC = small_server.c array_handling.c
EXE = small_server
OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(EXE)

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

clean: 
	$(RM) core $(EXE) $(OBJ)
