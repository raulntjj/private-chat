FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential

WORKDIR /app

COPY server.c .
RUN gcc server.c -o server -pthread

CMD [ "./server", "8080" ]
