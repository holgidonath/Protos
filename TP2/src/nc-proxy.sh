gnome-terminal --tab --title="pop3filter build" -- bash -c 'make clean; make all; ./pop3filter 127.0.0.1 -P 7000; $SHELL'
gnome-terminal --title="netcat server" -- bash -c 'sleep 1; echo \> PORT 7000; nc -l 7000; $SHELL'
gnome-terminal --title="netcat client" -- bash -c 'sleep 1; echo  \> PORT 1110; nc 127.0.0.1 1110; $SHELL'
