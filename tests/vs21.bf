# Verwendung von $(n) mit Transformationsregel

all:: t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12
  echo "Xx:$(5:t*=T-*)"
  echo "Xx:$(*:t*=X-*)"

t%::

#STDOUT:Xx:T-5
#STDOUT:Xx:X-1 X-2 X-3 X-4 X-5 X-6 X-7 X-8 X-9 X-10 X-11 X-12

