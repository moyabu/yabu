# Makros: Verwendung von "..."

# Makrodefinition
.define myrule t s...
$(.t:*=bin/%/*):: $(.s:*=tmp/%/*.o)
    echo "Xx: $(0).$(1)"
.enddefine

# Makroaufruf
.myrule target src1 src2

all:: bin/debug/target

tmp/%/%.o::
    echo "Xx: $(0)"


#STDOUT:Xx: tmp/debug/src1.o
#STDOUT:Xx: tmp/debug/src2.o
#STDOUT:Xx: bin/debug/target.tmp/debug/src1.o
