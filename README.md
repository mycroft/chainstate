# Bitcoin core chainstate parser

It is based on bitcoin core 0.15.1 client.

The bitcoin core's chainstate stores all blockchain's UTXOs. By parsing it, you can know where bitcoins are, how much are stored on each wallets, etc.

This parser handles all types of bitcoins addresses, like P2PKH (starting by 1), P2SH (starting by 1 or 3) and newer P2WPKH (bech32).

Some code was ripped of the Bitcoin core client, by the way. So, this software is under MIT licence.


# Deps

You need to get google's leveldb with C++ headers installed, or it won't compile/link.


# Build

```base
$ git submodule init
$ git submodule update
$ make
[...]
g++ -o chainstate chainstate.o hex.o varint.o pubkey.o -Lsecp256k1/.libs -lsecp256k1 -lcrypto -lleveldb -Llibbase58/.libs -lbase58 -Lbech32/ref/c -lbech32
$
```

If it doesn't build, you may have additional deps configured in a submodule. You will want to add those deps into the Makefile as well. Or you can also contribute by doing a proper Makefile ;) 


# How to run

You should stop bitcoin's client or daemon before copying the chainstate:

```bash
$ cp -Rp ~/.bitcoin/chainstate state
$ time ./chainstate >/tmp/cs.output 2>/tmp/cs.errors
real    8m36.144s
user    7m32.441s
sys     1m3.704s

$ head /tmp/cs.output
last block: 0000000000000000004e0f5635ad8b2e58ebd0a4f02c68c604d1b5697425ce72
eacfdcd42b27112ab6c8b435abec20181d05b0ba5d4f1829c002cc3ef0000000;1NwjXC31Enh5aqGHQbCtev9B7Rhk4knuEJ;1838
0118dd986e59473732239d39cb3b8890bf32677719dd8933b05f6614f4020000;32i3fvUTZkq2zeHBuosYDkiSCyMDhP62eo;132000
033e83e3204b0cc28724e147f6fd140529b2537249f9c61c9de9972750030000;1KaPHfvVWNZADup3Yc26SfVdkTDvvHySVX;65279
a53421b937be7bfe89ef6cc3f4124706b560af393b527e3e3d9d0c285b050000;1Lcd4mL7Zt53QTyR4wFJSksuyxCtfpTtws;2789

$ wc -l /tmp/cs.output /tmp/cs.errors
  59516004 /tmp/cs.output
    409643 /tmp/cs.errors
  59925647 total

$ grep 1F1tAaz5x1HUXrCNLbtMDqcw6o5GNn4xqX /tmp/cs.output | awk -F ';' '{sum += $3} END {print sum}'
1804638579


```

# Want to thank me ? More feature or more explanations ?

Please consider helping me:

- BTC: 3G734WzCrphZxN7afnrbwunZjV8MBqWUUV
- BCH: 1MQEd3csWAVRWcgVbqk8CoZYf312VM9vp1
- LTC: MEQA2uDajDiT3EyH1opRvNwywTDLvskLnq

