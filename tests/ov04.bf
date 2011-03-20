# Fehlererkennung: Weitere Zeichen nach "!configuration [cfg]"

!options
	cfg1

A=

!configuration [cfg1] x
  A=Ok

all:
  echo Xx:$(A)

#SHOULD_FAIL:S01
