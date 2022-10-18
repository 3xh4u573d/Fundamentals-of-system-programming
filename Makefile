CFLAGS=-Wno-unused-function -Wall -Wextra -Werror -O2
TARGETS= lab1mvnN3248 libmvnN3248.so

.PHONY: all clean

all: $(TARGETS)

clean:
rm -rf *.o $(TARGETS)

lab1mvnN3248: lab1mvnN3248.c plugin_api.h
gcc $(CFLAGS) -o lab1mvnN3248 lab1mvnN3248.c -ldl

libmvnN3248.so: libmvnN3248.c plugin_api.h
gcc $(CFLAGS) -shared -fPIC -o libmvnN3248.so libmvnN3248.c -ldl -lm