# Unbekannte Option.
# Fehler, wenn eine unbekannte Option benutzt wird.
# Variante mit "configuration".

CC=

!configuration unknown
    CC=gcc

all:

#SHOULD_FAIL:L01

~

