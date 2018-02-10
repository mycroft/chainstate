CPP=g++
CPPFLAGS=-Wall -I. -Ilibbase58 -Isecp256k1/include -Ibech32/ref/c
LDFLAGS=-Lsecp256k1/.libs -lsecp256k1 -lcrypto -lleveldb -Llibbase58/.libs -lbase58 -Lbech32/ref/c -lbech32
SRCS=chainstate.cc hex.cc varint.cc pubkey.cc
OBJS=$(SRCS:.cc=.o)
BINARY=chainstate

all: libbase58/.libs/libbase58.a secp256k1/.libs/libsecp256k1.a bech32/ref/c/libbech32.a clean $(BINARY)

$(BINARY): $(OBJS)
	$(CPP) -o $(BINARY) $(OBJS) $(LDFLAGS)

%.o: %.cc
	$(CPP) $(CPPFLAGS) -c -o $@ $<

libbase58/.libs/libbase58.a:
	cd libbase58 && ./autogen.sh && ./configure --enable-static --enable-shared=no && make

secp256k1/.libs/libsecp256k1.a:
	cd secp256k1 && ./autogen.sh && ./configure --enable-static --enable-shared=no && make

bech32/ref/c/libbech32.a:
	cd bech32/ref/c && $(CPP) -c -o segwit_addr.o segwit_addr.c && ar rcs libbech32.a segwit_addr.o

run:
	./$(BINARY) > /tmp/all.output 2> /tmp/all.errors

clean:
	rm -f $(BINARY) $(OBJS)

distclean: clean
	-cd libbase58 && make distclean
	-cd secp256k1 && make distclean
	-cd bech32/ref/c/ && rm -f *.o *.a