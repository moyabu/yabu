# Verschachtelung von $(VAR) und $(n)
# Die Ersetzung von $(n) erfolgt nach V-Substitution

OBJS=aa bb cc

xxx:: $(OBJS) ddd
   echo "Xx: $(0)-$(1)-$(4)"

aa bb cc ddd::

all: xxx
#STDOUT:Xx: xxx-aa-ddd
