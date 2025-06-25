FROM ubuntu:22.04

RUN apt-get update && apt-get install -y build-essential

WORKDIR /app

COPY client.c .
RUN gcc client.c -o client -pthread
