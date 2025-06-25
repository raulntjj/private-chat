FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y build-essential && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN gcc server.c -o server -pthread && \
    gcc client.c -o client -pthread

CMD [ "bash", "-c", "echo 'Imagem pronta. Execute o servidor ou o cliente. Exemplo: docker run ... chat-c-app ./server 8080' " ]
