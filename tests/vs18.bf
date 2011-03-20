# Variablen: Zusammensetzen von Werte mit ?=
#

# Startwert
VAR=eins zwei drei

# Wirkungslos, da "eins" schon enthalten
VAR?=eins

# "drei vier" anhängen
VAR?=drei vier


# Wirkungslos, da "drei drei" schon enthalten
VAR?=drei drei

all:
  echo "Xx-$(VAR)"
#STDOUT:Xx-eins zwei drei drei vier
