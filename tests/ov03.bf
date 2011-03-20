# Neue Syntax "!configuration [cfg]"

!options
	cfg1

!configure fixed
	fixed: cfg1

A=

!configuration [cfg1]
  A=Ok

all:
  echo Xx:$(A)

#STDOUT:Xx:Ok
