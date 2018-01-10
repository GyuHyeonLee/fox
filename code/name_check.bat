@echo off

echo -------
echo -------

set Wildcard=*.h *.cpp *.inl *.c

echo NAMES FOUND:
findstr -s -n -i -l %1 %Wildcard%

echo -------
echo -------
