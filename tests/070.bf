# for-Anweisung: Verschachtelung

.foreach a 1 2
.foreach b x $(.a)
V$(.a)$(.b)=Wert$(.a)$(.b)
.endforeach
.endforeach

all:
	echo Xx$(V1x)
	echo Xx$(V11)
	echo Xx$(V2x)
	echo Xx$(V22)

#STDOUT:XxWert1x
#STDOUT:XxWert11
#STDOUT:XxWert2x
#STDOUT:XxWert22
