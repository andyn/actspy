#!/bin/bash

cd /root/build
mkdir -p /root/dist
gcc -std=c17 -pedantic -Wall -Wextra -o /root/dist/actspy actspy.c
