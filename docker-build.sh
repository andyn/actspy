#!/bin/bash

mkdir dist

docker build . -t actspy-builder:latest

docker run -it -v $(pwd)/dist:/root/dist actspy-builder:latest