# Verwendung von !serialize
#
# Die Reihenfolge in der !serialize-Anweisung hat keine Bedeutung


!serialize 4.test 2.% 3.% 1.test

all:: 4.test 3.test 2.test 1.test

%.test::
    echo "Xx:$(0) begin"
    sleep 0.% 2>/dev/null || sleep %
    echo "Xx:$(0) end"

#PARALLEL:4
#STDOUT:Xx:4.test begin
#STDOUT:Xx:4.test end
#STDOUT:Xx:3.test begin
#STDOUT:Xx:3.test end
#STDOUT:Xx:2.test begin
#STDOUT:Xx:2.test end
#STDOUT:Xx:1.test begin
#STDOUT:Xx:1.test end

