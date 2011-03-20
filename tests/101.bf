# Konfigurationsabhängige Quellen.
# Eine mit @xxx gewählte Konfiguration ist bereits aktiv, wenn Yabu die Liste
# der Quellen betrachtet. Das kann relevant sein, falls in der Quellenliste eine
# konfigurationsbhängige Variable vorkommt.

OBJS=
!options
	all
!configuration all
    OBJS=abc

# Diese Regel wird für das Ziel "all" benutzt. Damit wird zugleich die
# Konfiguration "all" ausgewählt.
%:: [%] $(OBJS)
   echo "Xx: $(0)-$(1)"

abc::

#STDOUT:Xx: all-abc
