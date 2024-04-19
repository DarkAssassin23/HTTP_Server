#!/bin/bash

# Create 150, or the first argument if it is larger, clients to connect to
# the server
clients=150
if [[ $# -gt 0 ]] ; then
	if [[ $clients -lt $1 ]] ; then
		clients=$1
	fi
fi

for (( i=0; i<$clients; i++));
do
	python3 basicClient.py $@ &
done
wait
