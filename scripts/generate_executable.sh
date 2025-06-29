#!/bin/bash
# Usage: ./generate_executable.sh <output_executable> [-s]

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <output_executable> [-s]"
    exit 1
fi

EXE_NAME="$1"
KEEP_TMP=0
if [ "$2" == "-s" ]; then
    KEEP_TMP=1
fi

# Assemble all .s files in tmp/ to .o
for SFILE in tmp/*.s; do
    [ -e "$SFILE" ] || continue
    OFILE="${SFILE%.s}.o"
    arm-none-eabi-as -g -o "$OFILE" "$SFILE"
    if [ $? -ne 0 ]; then
        echo "Assembly failed for $SFILE."
        exit 2
    fi
done

# Collect all object files in tmp/
TMP_OBJS=$(find tmp -name '*.o' 2>/dev/null | tr '\n' ' ')

ELF="tmp/${EXE_NAME}.elf"

# Link
arm-none-eabi-gcc -specs=rdimon.specs -lc -lrdimon -o "$ELF" $TMP_OBJS
if [ $? -ne 0 ]; then
    echo "Linking failed."
    [ $KEEP_TMP -eq 0 ] && rm -f $TMP_OBJS
    exit 3
fi

# Move ELF to project root with requested name
mv "$ELF" "./$EXE_NAME"

# Clean up tmp directory
if [ $KEEP_TMP -eq 0 ]; then
    rm -rf tmp/
fi

exit 0