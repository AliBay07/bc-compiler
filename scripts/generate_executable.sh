#!/bin/bash
# Usage: ./generate_executable.sh <input.s|input.o> <output_executable> [-s]

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
    echo "Usage: $0 <input.s|input.o> <output_executable> [-s]"
    exit 1
fi

INPUT="$1"
EXE_NAME="$2"
KEEP_TMP=0
if [ "$3" == "-s" ]; then
    KEEP_TMP=1
fi

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

ELF="tmp/${EXE_NAME}.elf"

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

# Link
arm-none-eabi-gcc -specs=rdimon.specs -lc -lrdimon -o "$ELF" $TMP_OBJS
if [ $? -ne 0 ]; then
    echo "Linking failed."
    [ $KEEP_TMP -eq 0 ] && rm -f $TMP_OBJS
    exit 3
fi

# Move ELF to project root with requested name
mv "$ELF" "./$EXE_NAME"

# Clean up intermediate files if not keeping
if [ $KEEP_TMP -eq 0 ]; then
    rm -f $TMP_OBJS
    find tmp -name '*.s' -delete
fi

exit 0