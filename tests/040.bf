# include: Dateiname ist relativ zur aktuellen Datei

# Der folgende Dateiname wir relativ zu dieser Datei interpertiert.
# 040.a wiederum enthält ein ".include 040.b", welches relativ zu include/040a
# interpretiert wird.

.include include/040.a

#STDOUT:XxOk
