OBJ=systest.o fault_model.o extra.o db_sym.o db_disasm.o db_access.o read_nlist.o
OBJ_RND=rnd.o
OBJ_SOCKET=socket.o

DEFINES=-DCONFIG_SWIFI
CFLAGS=$(DEFINES)

all:	swifi rnd socket

swifi:	$(OBJ)
	$(CC) -o swifi $(OBJ)

rnd:	$(OBJ_RND)
	$(CC) -o $@ $(OBJ_RND)

socket:	$(OBJ_SOCKET)
	$(CC) -o $@ $(OBJ_SOCKET)

clean:
	rm -f swifi $(OBJ)
