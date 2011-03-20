# Makros: Makroargumente mehrfach definiert
#
# Ein Makroargument darf beim Aufruf nicht mehrfach übergeben werden.

.define macro arg1 arg2 arg3
.enddefine

.macro
  arg1=1
  arg2=2
  arg1=3

#SHOULD_FAIL:L03
