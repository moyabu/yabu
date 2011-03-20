# Optionaler Wert für unbekannte Variable.
# Einer Variable kann nur dann ein optionaler Wert zugewiesen werden,
# wenn die Variable bereits definiert ist.

!options
	cfg

TESTVAR[cfg]=abc

all:

#SHOULD_FAIL:L01

