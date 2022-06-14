#!/bin/bash

rm -rf files
mkdir files
cd files
echo "pozdrawiam czytelnikow" > file.txt
ln file.txt file_hard.txt
ln -s file.txt file_soft.txt
mkdir create
mkdir create/inside
mkdir move1
touch move1/move.txt
mkdir move2
cd ..

rm -f test test.o c_stuff.o
clang++ -c -o test.o test.cpp -std=c++11
clang -c -o c_stuff.o c_stuff.c
clang++ -o test test.o c_stuff.o
./test
