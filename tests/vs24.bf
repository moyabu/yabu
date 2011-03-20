# Alternative Platzhalter
# Folgende Zeichen sind als alternative Platzhalter erlaubt: @ & ^ ~ + #

A=1 2 3

all:
    echo "Xx:$(A:*=1-*)"
    echo "Xx:$(A@:@=2-@)"
    echo "Xx:$(A&:&=3-&)"
    echo "Xx:$(A^:^=4-^)"
    echo "Xx:$(A~:~=5-~)"
    echo "Xx:$(A+:+=6-+)"
    echo "Xx:$(A#:#=7-#)"

#STDOUT:Xx:1-1 1-2 1-3
#STDOUT:Xx:2-1 2-2 2-3
#STDOUT:Xx:3-1 3-2 3-3
#STDOUT:Xx:4-1 4-2 4-3
#STDOUT:Xx:5-1 5-2 5-3
#STDOUT:Xx:6-1 6-2 6-3
#STDOUT:Xx:7-1 7-2 7-3
