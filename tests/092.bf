# '%' gilt maximal bis zum ersten '.' oder '/'
# Das bewirkt, das die erste Regel (%wei) nicht für eins.zwei und eins/zwei
# anwendbar ist.

%wei::
    echo Xx=1==%1

%.%::
    echo Xx==%1=.=%2

%/%::
    echo Xx==%1=/=%2

all: eins-zwei eins.zwei eins/zwei

#STDOUT:Xx=1==eins-z
#STDOUT:Xx==eins=.=zwei
#STDOUT:Xx==eins=/=zwei

