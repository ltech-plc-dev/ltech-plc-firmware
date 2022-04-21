FROM ubuntu:latest

RUN apt-get update

RUN apt-get install libssl-dev -y
RUN apt-get install iproute2 -y
RUN apt-get install python3 -y
RUN apt-get install python3-pip -y

RUN echo "plc:plc" | chpasswd

WORKDIR /home/plc

COPY firmware.bin .

RUN chmod +x /home/root/firmware.bin

RUN chown -R root:root /home/root

USER root

EXPOSE 8085

CMD /home/root/firmware.bin
