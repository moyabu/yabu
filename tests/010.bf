# Pro Variable kann es nur eine optionale Zuweisung (=) geben. 
# In diesem Beispiel sind es zwei -> Fehler


!options
	eins zwei

# Widersprüchliche Zuweisungen für +eins und +zwei
CFLAGS=
CFLAGS[eins]=-eins
CFLAGS[zwei]=-zwei

# Option "eins" einschalten
!configure x
   x: eins

# Option "zwei" ebenfalls einschalten --> Fehler in $(CFLAGS)
all: [zwei]
	echo "$(CFLAGS)"

#SHOULD_FAIL:L35

