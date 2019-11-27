nahrát bin do /home/pi
upravit verzi XXXX
spustit v příkazové řádce

CLOCK1
sudo python /root/.arduino15/packages/esp8266/hardware/esp8266/2.4.2/tools/espota.py -i 192.168.1.58 -p 8266 --auth=password -f /home/pi/Clock.ino.d1_mini.bin
CLOCK2
sudo python /root/.arduino15/packages/esp8266/hardware/esp8266/2.4.2/tools/espota.py -i 192.168.1.57 -p 8266 --auth=password -f /home/pi/Clock.ino.d1_mini.bin