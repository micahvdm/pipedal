#!/bin/bash 

set -x

ipaddr=`hostname -I`
echo $ipaddr

sudo apt install -y liblilv-dev libboost-dev jackd2 libjack-jackd2-dev libnl-3-dev libnl-genl-3-dev libsystemd-dev catch libasound2-dev uuid-dev libwebsocketpp-dev authbind libavahi-client-dev
curl -fsSL https://deb.nodesource.com/setup_14.x | sudo -E bash -
sudo apt install -y nodejs

wget https://github.com/rerdavies/pipedal/releases/download/v1.0.15/pipedal_1.0.15_arm64.deb
sudo dpkg --install pipedal_1.0.15_arm64.deb

sudo pipedalconfig --install --port $ipaddr:80 --prefix /usr

sudo cp jack.service /usr/lib/systemd/system
sudo adduser --no-create-home --system --group jack
sudo adduser pistomp jack --quiet
sudo adduser root jack --quiet
sudo adduser jack audio --quiet
sudo cp jackdrc /etc/
sudo chmod +x /etc/jackdrc
sudo chown jack:jack /etc/jackdrc
