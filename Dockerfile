FROM gcc:latest as builder

RUN apt update && apt install -y cmake valgrind

WORKDIR /app

COPY . .

RUN rm -rf build

RUN mkdir build

WORKDIR /app/build

RUN cmake ..

RUN make

#ENTRYPOINT valgrind -s --track-fds=yes --track-origins=yes --leak-check=full --show-leak-kinds=all /app/build/src/dncs
ENTRYPOINT /app/build/src/dncs
