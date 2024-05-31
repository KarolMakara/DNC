FROM gcc:latest as builder

RUN apt update && apt install -y cmake


WORKDIR /app

COPY . .

RUN rm -rf build

RUN mkdir build

WORKDIR /app/build

RUN cmake ..

RUN make

FROM gcc AS run-time
COPY --from=builder /app/build/src/dncs /usr/bin/

ENTRYPOINT /usr/bin/dncs