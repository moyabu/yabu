# Variablen: Zusammensetzen von Werte mit ?=
VAR=eins zwei drei
VAR?=eins
VAR?=zwei
VAR?=drei
VAR?=vier
all:
  echo "Xx-$(VAR)"
#STDOUT:Xx-eins zwei drei vier
