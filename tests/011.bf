# Optionaler Wert f�r unbekannte Variable.
# Einer Variable kann nur dann ein optionaler Wert zugewiesen werden,
# wenn die Variable bereits definiert ist.

!options
	cfg
!configuration cfg
    TESTVAR=abc

all:

#SHOULD_FAIL:L01

