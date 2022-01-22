FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --allow-unauthenticated \
    clang   \
    g++     \
    make && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ADD Makefile /Makefile
ADD main.cpp /main.cpp

RUN CXX=clang++ make
