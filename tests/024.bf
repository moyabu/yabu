# Syntax in !options
#
# Ungewöhnliche, aber zulässige Schreibweisen.


!options
    sel1(   a b c  )sel2(	d	e)

!configure x
    x: +a+d

all: 
    echo Xx:$(_sel1)/$(_sel2)

#STDOUT:Xx:a/d
