all:
	@rm -f chainstate
	@g++ -o chainstate chainstate.cc hex.cc varint.cc lleveldb
