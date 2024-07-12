#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

typedef struct string {
	char* str;
	int len;
} string;
int serial_open(char* name){
	int serial_port = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (serial_port < 0) {
		fprintf(stderr,"Error %i from open: %s\n", errno, strerror(errno));
		return 0;
	}

	struct termios tty;

	if (tcgetattr(serial_port, &tty) != 0) {
		fprintf(stderr,"Error %i from tcgetattr: %s\n", errno, strerror(errno));
		return 1;
	}

	tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
	tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
	tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
	tty.c_cflag |= CS8; // 8 bits per byte (most common)
	tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO; // Disable echo
	tty.c_lflag &= ~ECHOE; // Disable erasure
	tty.c_lflag &= ~ECHONL; // Disable new-line echo
	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	tty.c_iflag &= ~(ICRNL | INLCR); // Disable CR-to-NL mapping

	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

	tty.c_cc[VTIME] = 10;	// Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
	tty.c_cc[VMIN] = 0;

	cfsetispeed(&tty, 2400);
	cfsetospeed(&tty, 2400);

	if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
		fprintf(stderr,"Error %i from tcsetattr: %s\n", errno, strerror(errno));
		return 0;
	}
	return serial_port;
}
int serial_close(int serial_port){
	close(serial_port);
	return 0;
}
int serial_write(int fp, string data){
	return write(fp, data.str, data.len);
}
string serial_read(int fp){
	// Read from serial port
	char read_buf [256]={0};
	int num_bytes = 0;
	time_t ntime=time(NULL);
	string ret={0};
	while(ntime+2>time(NULL)){
		int len=read(fp, &read_buf, sizeof(read_buf));
		if(len<=0){
			usleep(10000);
			continue;
		}
		if(!ret.len)
			ret.str=malloc(len);
		else
			ret.str=realloc(ret.str,ret.len+len);
		memcpy(ret.str+ret.len,read_buf,len);
		int oldlen=ret.len;
		ret.len+=len;
		if(memchr(ret.str+oldlen,'\r',ret.len-oldlen)) break;
	}
	return ret;
}

uint16_t calc_crc(uint8_t  *pin, uint8_t len){
	uint16_t crc;
	uint8_t da;
	uint8_t  *str;
	uint8_t bCRCHign;
	uint8_t bCRCLow;
	uint16_t crc_ta[16]={
		0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
		0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
	};
	str=pin;
	crc=0;
	while(len--!=0){
		da=((uint8_t)(crc>>8))>>4;
		crc<<=4;
		crc^=crc_ta[da^(*str>>4)];
		da=((uint8_t)(crc>>8))>>4;
		crc<<=4;
		crc^=crc_ta[da^(*str&0x0f)];
		str++;
	}
	bCRCLow = crc;
	bCRCHign= (uint8_t)(crc>>8);
	if(bCRCLow==0x28||bCRCLow==0x0d||bCRCLow==0x0a){
		bCRCLow++;
	}
	if(bCRCHign==0x28||bCRCHign==0x0d||bCRCHign==0x0a){
		bCRCHign++;
	}
	crc = ((uint8_t)bCRCHign)<<8;
	crc += bCRCLow;
	return crc;
}
string cmd_crc(string cmd){
	string ret=(string){malloc(cmd.len+3),cmd.len+3};
	strcpy(ret.str,cmd.str);
	uint16_t crc=calc_crc(cmd.str,cmd.len);
	char* temp=(void*)&crc;
	ret.str[cmd.len+0]=temp[1];
	ret.str[cmd.len+1]=temp[0];
	ret.str[cmd.len+2]='\r';
	return ret;
}
string iquery(string cmd){
	if(!cmd.len) return (string){0};
	int fp=serial_open("/dev/ttyUSB0");
	if(!fp) return (string){0};
	string cc=cmd_crc(cmd);
	serial_write(fp,cc);
	free(cc.str);
	string ret=serial_read(fp);
	serial_close(fp);
	return ret;
}
int usage(){
	fprintf(stderr,"Usage: voltronic <cmd>\n");
	return 0;
}
string c_s(char* in){
	return (string){in,strlen(in)};
}
string today(){
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	string ret=(string){malloc(11),10};
	strftime(ret.str, 11, "%Y-%m-%d %H:%M:%S", tm_info);
	return ret;
}
string now(){
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	string ret=(string){malloc(9),8};
	strftime(ret.str, 9, "%H:%M:%S", tm_info);
	return ret;
}
string iquery2(string in){
	int tries=3;
	while(tries--){
		string ret=iquery(in);
		if(ret.len && ret.str[0]=='(' && ret.str[ret.len-1]=='\r'){
			string ret2=(string){malloc(ret.len-4),ret.len-4};
			memcpy(ret2.str,ret.str+1,ret.len-4);
			free(ret.str);
			return ret2;
		}
		if(ret.len) free(ret.str);
		usleep(10000);
	}
	return (string){0};
}
string iquery3(){
	string val1=iquery2(c_s("QMOD"));
	string val2=iquery2(c_s("QPIGS"));
	string val3=iquery2(c_s("QPIWS"));
	if(!val1.len||!val2.len||!val3.len) return (string){0};
	string val4=today();
	string val5=now();
	string ret={0};
	ret.len=val1.len+val2.len+val3.len+val4.len+val5.len+4;
	ret.str=malloc(ret.len+1);
	sprintf(ret.str,"%.*s %.*s %.*s %.*s %.*s",val4.len,val4.str,val5.len,val5.str,val1.len,val1.str,val2.len,val2.str,val3.len,val3.str);
	free(val1.str);
	free(val2.str);
	free(val3.str);
	free(val4.str);
	free(val5.str);
	return ret;
}
int stop=0;
void stop_exec(int signal){
	if(signal==SIGTERM) stop=1;
}
int iquery4(){
	signal(SIGTERM, stop_exec);
	time_t t=time(NULL);
	while(!stop){
		string ret=iquery3();
		if(ret.len){
			printf("%.*s\n",ret.len,ret.str);
			fflush(stdout);
			free(ret.str);
		}
		int sleep=5-(time(NULL)-t);
		usleep(sleep*1000000);
		t=time(NULL);
	}
	return 0;
}
int main(int argc,char** argv){
	if(argc==2){
		string ret=iquery2((string){argv[1],strlen(argv[1])});
		if(!ret.len) return -1;
		printf("%.*s\n",ret.len,ret.str);
		free(ret.str);
	}
	else if(argc==1){
		return iquery4();
	}
	else usage();
	return 0;
}
