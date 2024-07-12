all:
	gcc -o voltronic voltronic.c

release:
	gcc -o voltronic voltronic.c -O2
install:
	sudo cp voltronic.service /etc/systemd/system/
	sudo systemctl enable voltronic
	sudo systemctl restart voltronic
	sudo systemctl status voltronic
reinstall:
	sudo cp voltronic.service /etc/systemd/system/
	sudo systemctl daemon-reload
	echo Installed, now run : restart voltronic
