#!/bin/bash
RM=/bin/rm
doxygen Doxyfile
cd html
for name in `/bin/ls *.dot`
do
    stub=`echo $name | cut -f1 -d.`
    dot $name -Tpng -o ${stub}.png
    $RM $name
done
$RM FreeSans.ttf
