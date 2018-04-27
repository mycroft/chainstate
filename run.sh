#!/bin/sh

COIN=bitcoin
USE_TESTNET=0

if test ${USE_TESTNET} -eq 1; then
    COIN_NAME=${COIN}-testnet
else
    COIN_NAME=${COIN}
fi

PIDFILE=$HOME/var/pid/${COIN_NAME}.pid
BTC_SCRIPT=$HOME/bin/${COIN_NAME}.sh
SCRIPT=$(cd $(dirname $0); /bin/pwd)

BALANCES_FILE=balances.out

if test ! -e ${PIDFILE}; then
    echo "pid file doesn't exist."
    exit 1
fi

cd ${SCRIPT}
pid=$(cat ${PIDFILE})

echo "Cleaning existing files..."
rm -f cs.out cs.err ${BALANCES_FILE}.gz

ps ${pid} >/dev/null
if test $? -eq 0; then
    echo "Stopping ${COIN}d..."
    kill -2 ${pid}
fi

while ps ${pid} >/dev/null; do
    sleep 1
done

echo "Copying chainstate..."
if test ${USE_TESTNET} -eq 1; then
    cp -Rp ~/.${COIN}/testnet3/chainstate state
else
    cp -Rp ~/.${COIN}/chainstate state
fi

echo "Syncing..."
sync

echo "Copying done. Restarting ${COIN}d..."
${BTC_SCRIPT}

echo "Running chainstate parser..."
./chainstate ${COIN_NAME} >cs.out 2>cs.err

echo "Generated output:"
ls -l cs.out cs.err

if test ! -e cs.out; then
    echo "Missing input file (cs.out)"
    exit 1
fi

echo "Generating & sorting final balances..."
cut -d';' -f3,4 cs.out | \
    sort | \
    awk -F ';' '{ if ($1 != cur) { if (cur != "") { print cur ";" sum }; sum = 0; cur = $1 }; sum += $2 } END { print cur ";" sum }' | \
    sort -t ';' -k 2 -g -r > ${BALANCES_FILE}

echo "Compressing balances"
gzip ${BALANCES_FILE}

echo "Generated archive:"
ls -l ${BALANCES_FILE}.gz

echo "Cleaning state"
rm -fr state

exit 0
