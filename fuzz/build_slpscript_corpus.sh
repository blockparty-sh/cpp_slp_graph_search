#!/bin/bash

cat ../build-afl/slp-unit-test-data/src/slp-unit-test-data/script_tests.json  | jq -r ".[] | .script" | while read script;
do
    h=`echo $script | sha256sum - | awk '{ print $1 }'`
    echo $script | xxd -r -p - slpscript-corpus-pre/${h}.tx;
done

afl-cmin -i slpscript-corpus-pre -o slpscript-corpus -- ./../build-afl/bin/fuzz_crash_slpparse @@
