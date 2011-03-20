# Verschachtelte Makroaufrufe
#
# Ein Makro kann ein anderes aufrufen, und Argumente können
# den gleichen Namen haben


.define inner args
all:: $(.args:*=test-*)
.enddefine

# Das Makro «aussen» ruft «inner» auf
.define aussen args
.inner
   args=$(.args)
.inner
    args=$(.args:*=*-2)
.enddefine

.aussen
   args=1 2 3

test-%::
    echo "Xx:$(0)"

#STDOUT:Xx:test-1
#STDOUT:Xx:test-2
#STDOUT:Xx:test-3
#STDOUT:Xx:test-1-2
#STDOUT:Xx:test-2-2
#STDOUT:Xx:test-3-2

