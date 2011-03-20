# '%%' wird durch einen möglichst langen Substring ersetzt.
# Der Telstring kann auch '/' und '.' umfassen.

%%/%%.%::
    echo Xx==%1==%2==%3


all: eins/zwei.drei aaa/bbb/ccc/ddd.eee a/b.c.d.e.f

#STDOUT:Xx==eins==zwei==drei
#STDOUT:Xx==aaa/bbb/ccc==ddd==eee
#STDOUT:Xx==a==b.c.d.e==f
