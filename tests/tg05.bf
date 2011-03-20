# !serialize: Verwendung von "<...>"
#
# 


!serialize <group1> 1.% 
!serialize <group1> 2.% 
!serialize <group1> 3.% 
!serialize <group1> 4.% 

all:: 1.test 2.test 3.test 4.test

%.test::
    echo "Xx:$(0) start"
    sleep 0.% 2>/dev/null || sleep %
    echo "Xx:$(0) end"

#PARALLEL:4
#STDOUT:Xx:1.test start
#STDOUT:Xx:1.test end
#STDOUT:Xx:2.test start
#STDOUT:Xx:2.test end
#STDOUT:Xx:3.test start
#STDOUT:Xx:3.test end
#STDOUT:Xx:4.test start
#STDOUT:Xx:4.test end

