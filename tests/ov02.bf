# Kombination von -OPT und +OPT
# Eine optionale Wertzuweisung kann nicht gleichzeitg +OPT und -OPT enthalten.

CC=

!options
	a b c
!configuration a -b
    CC[b-c]=/usr/bin/gcc

#SHOULD_FAIL:L24

