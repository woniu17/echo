NAME=client
OBJS=client.o
CFLAGS=-Wall -Wextra -Werror -lpthread -lev -I. -g -O0 -std=c99

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

client.o: client.c

clean:
	rm -rf *.o
	rm -f $(NAME)

PHONY: run

