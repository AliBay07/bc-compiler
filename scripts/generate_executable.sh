#!/bin/bash
# Usage: ./generate_executable.sh <file.s|file.o>

if [ $# -ne 1 ]; then
    echo "Usage: $0 [--gdb] <file.s|file.o>"
    exit 1
fi

INPUT="$1"
BASENAME="${INPUT%.*}"

# Assemble if .s file
if [[ "$INPUT" == *.s ]]; then
    OBJ="${BASENAME}.o"
    arm-none-eabi-as -g -o "$OBJ" "$INPUT"
    if [ $? -ne 0 ]; then
        echo "Assembly failed."
        exit 2
    fi
elif [[ "$INPUT" == *.o ]]; then
    OBJ="$INPUT"
else
    echo "Input must be a .s or .o file"
    exit 1
fi

ELF="${BASENAME}.elf"

# Assemble all .s files in lib/ to .o
for SFILE in lib/*.s; do
    [ -e "$SFILE" ] || continue
    OFILE="${SFILE%.s}.o"
    arm-none-eabi-as -g -o "$OFILE" "$SFILE"
    if [ $? -ne 0 ]; then
        echo "Assembly failed for $SFILE."
        exit 2
    fi
done

# Collect all object files in lib/
LIB_OBJS=$(find lib -name '*.o' 2>/dev/null | tr '\n' ' ')

# Link
arm-none-eabi-gcc -specs=rdimon.specs -lc -lrdimon -o "$ELF" "$OBJ" $LIB_OBJS
if [ $? -ne 0 ]; then
    echo "Linking failed."
    # Clean up
    rm -f "$OBJ" $LIB_OBJS
    exit 3
fi

# Clean up intermediate files
rm -f "$OBJ" $LIB_OBJS

exit 0