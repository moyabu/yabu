# Optionsnamen müssen eindeutig sein.
#
# Es ist ein Fehler, wenn der gleiche Wert in zwei Auswahloptionen vorkommt.


!options
	cfg1(a b c)
	cfg2(b c d)

all: 

#SHOULD_FAIL:L03
