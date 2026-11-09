#!/bin/sh
{
    echo "count: $#"
    echo "file: $1"
    if [ -t 0 ]; then echo "stdin: terminal"; else echo "stdin: other"; fi
} > "$1.args.tmp"
mv "$1.args.tmp" "$1.args"
