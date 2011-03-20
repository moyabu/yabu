# Fehlerkennung in Konfigurationsauswahl 
#
# Alle auf ein Ziel passenden Regeln müssen eine gültige
# Konfiguration auswählen. Der Text innerhalb [...] muß
# also eine syntaktisch korrekte Konfiguration sein und
# darf keine unbekannten Optionen enthalten.
# Andernfalls tritt ein Fehler auf, und zwar auch dann
# wenn die betreffende Regel gar nicht ausgewählt würde.


all:: target-x

# Diese Regel würde ausgewählt (höchste Prioriät)
target-x::
   echo $(0)

# Diese Regel würde gar nicht benutzt. Sie führt dennoch zu
# einem Fehler, da es keine Option 'x' gibt.
target-%:: [%]
   echo $(0)


#SHOULD_FAIL:L01
