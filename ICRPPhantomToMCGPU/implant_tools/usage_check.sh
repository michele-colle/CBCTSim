#!/bin/bash
# Reads the last usage reading the Windows monitor logged (it polls every 10 min,
# so this can be up to 10 min stale -- the margin to the 98%/75% kill absorbs it).
L=/mnt/c/Users/colle/claude_usage/claude_usage_log.txt
LINE=$(grep 'Current session:' "$L" | tail -1)
S=$(sed -n 's/.*Current session: \([0-9]*\)%.*/\1/p' <<<"$LINE")
Wk=$(sed -n 's/.*Current week: \([0-9]*\)%.*/\1/p' <<<"$LINE")
TS=$(cut -d' ' -f1-2 <<<"$LINE")
STOP=no
[ "${S:-100}" -ge 80 ] && STOP=yes
[ "${Wk:-100}" -ge 70 ] && STOP=yes
echo "AT=$TS SESSION=${S}% WEEK=${Wk}% STOP=$STOP  (kill fires at session>=98 or week>=75)"
