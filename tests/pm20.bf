# Makros: locale Makros mit "local:"

.include pm20.incl

.define mkrule tgt
global_$(.tgt)::
   echo "Xx:$(0)"
.enddefine

.mkrule main


#STDOUT:Xx:global_main
