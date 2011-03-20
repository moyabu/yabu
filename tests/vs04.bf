# Ersetzungsmodus '?' und '!'
# Im Modus '?' werden nicht passende Wörter unverändert übernommen.
# Im Modus '!' werden nicht passende Wörter unterdrückt.

TEST=aaa.1 bbb.1 ccc.2 ddd.3 eee.1

all:
  echo Xx:$(TEST!:*.1=1.*)
  echo Xx:$(TEST!:*.2=2.*)
  echo Xx:$(TEST!:*.3=3.*)

  echo Xx:$(TEST?:*.1=1.*)
  echo Xx:$(TEST?:*.2=2.*)
  echo Xx:$(TEST?:*.3=3.*)

#STDOUT:Xx:1.aaa 1.bbb 1.eee
#STDOUT:Xx:2.ccc
#STDOUT:Xx:3.ddd
#STDOUT:Xx:1.aaa 1.bbb ccc.2 ddd.3 1.eee
#STDOUT:Xx:aaa.1 bbb.1 2.ccc ddd.3 eee.1
#STDOUT:Xx:aaa.1 bbb.1 ccc.2 3.ddd eee.1
#
