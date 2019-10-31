FROM debian:buster-slim AS builder

RUN DEBIAN_FRONTEND=noninteractive \ 
    apt update -q && \ 
    apt upgrade --yes -q && \
    apt install --yes -q build-essential && \
    apt autoremove && \
    apt clean

COPY actspy.c /root/build/
COPY entrypoint.sh /root/
WORKDIR /root/

CMD ["/root/entrypoint.sh"]