#!/bin/sh

# set -x
set +e

SARIFDIR="$1"
mkdir -p "$SARIFDIR"

shift

INVOCATIONHASH=$(echo "$*" | sha1sum --text - | cut -d' ' -f 1)

LOGNAME="$SARIFDIR/$INVOCATIONHASH.json"

$@ -fdiagnostics-format=sarif -Wno-sarif-format-unstable 2>"$LOGNAME"
RES=$?

LC=$(wc -l "$LOGNAME" | cut -d' ' -f 1)
if [ $LC -eq 3 ]; then
   # Got no warnings.
   exit $RES
elif [ $LC -eq 4 ]; then
   OUT="$(cat "$LOGNAME")"
   # Drop last line, which is: "$N (warning|error)[s] generated."
   echo "$OUT" | head -n 2 | tail -n 1 > "$LOGNAME"
   exit $RES
else
   /bin/false # ???
fi
