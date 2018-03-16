#!/bin/sh

PIDFILE=$HOME/var/pid/bitcoin.pid
BTC_SCRIPT=$HOME/bin/bitcoin.sh
SCRIPT=$(cd $(dirname $0); /bin/pwd)

BALANCES_FILE=balances.out

if test ! -e ${PIDFILE}; then
    echo "pid file doesn't exist."
fi

cd ${SCRIPT}
pid=$(cat ${PIDFILE})

echo "Cleaning existing files..."
rm -f cs.out cs.err ${BALANCES_FILE}.gz

ps ${pid} >/dev/null
if test $? -eq 0; then
    echo "Stopping bitcoind..."
    kill -2 ${pid}
fi

while ps ${pid} >/dev/null; do
    sleep 1
done

echo "Copying chainstate..."
cp -Rp ~/.bitcoin/chainstate state

echo "Syncing..."
sync

echo "Copying done. Restarting bitcoind..."
${BTC_SCRIPT}

echo "Running chainstate parser..."
./chainstate >cs.out 2>cs.err

echo "Generated output:"
ls -l cs.out cs.err

if test ! -e cs.out; then
    echo "Missing input file (cs.out)"
    exit 1
fi

echo "Generating final balances..."
cut -d';' -f2,3 cs.out | \
    sort | \
    awk -F ';' '{ if ($1 != cur) { if (cur != "") { print cur ";" sum }; sum = 0; cur = $1 }; sum += $2 } END { print cur ";" sum }' \
    > ${BALANCES_FILE}

echo "Sorting balances"
sort -t ';' -k 2 -g -r ${BALANCES_FILE}

echo "Compressing balances"
gzip ${BALANCES_FILE}

echo "Generated archive:"
ls -l ${BALANCES_FILE}.gz

echo "Cleaning state"
rm -fr state

exit 0
