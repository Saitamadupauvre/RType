#!/bin/sh
echo "args: $*"
echo "to stderr" >&2
printf "no newline"
exit 3
