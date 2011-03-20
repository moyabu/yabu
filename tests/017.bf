# Optionsnamen müssen eindeutig sein.
#
# Es ist ein Fehler, wenn eine boolsche Option als Wert einer Auswahloption benutzt wird.


!options
	cfg1 cfg2(b c cfg1)

all: 

#SHOULD_FAIL:L03
