#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_MAX 1024
#define VALUE_MAX 256
// #define LED_PIN  17

#define DIRECTION_MAX 45
#define MAX_OUTPUT_LENGTH 256

#define MOTOR_ON 350000
#define MOTOR_OFF 2500000

#define IN 0
#define OUT 1
#define PWM 0

#define LOW 0
#define HIGH 1
#define VALUE_MAX 256

#define POUT 17

char ControlMessage[VALUE_MAX];
int clnt_sock = -1;

static int PWMExport(int pwmnum){
    char buffer[BUFFER_MAX];
    int bytes_written;
    int fd;

    fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
    if(-1==fd){
        fprintf(stderr, "Failed to open in export!\n");
        return(-1);
    }
    bytes_written=snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
    write(fd, buffer, bytes_written);
    close(fd);
    sleep(1);
    return(0);
}

static int PWMUnexport(int pwmnum)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/pwm/pwmchip0/unexport", O_WRONLY);
    if(-1==fd){
        fprintf(stderr, "Failed to open in unexport!\n");
        return(-1);
    }
    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
    write(fd, buffer, bytes_written);
    close(fd);
    sleep(1);
    return(0);
}
static int PWMEnable(int pwmnum){
    static const char s_unenable_str[] = "0";
    static const char s_enable_str[] = "1";
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
    fd=open(path,O_WRONLY);
    if(-1==fd){
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd,s_unenable_str,strlen(s_unenable_str));
    close(fd);

    fd=open(path,O_WRONLY);
    if(-1==fd){
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd,s_enable_str,strlen(s_enable_str));
    close(fd);
    return 0;
}

static int PWMUnable(int pwmnum){
    static const char s_unable_str[] = "0";
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
    fd=open(path,O_WRONLY);
    if(-1==fd){
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd,s_unable_str,strlen(s_unable_str));
    close(fd);
    return 0;
}

static int PWMWritePeriod(int pwmnum, int value){
    char s_values_str[VALUE_MAX];
    char path[VALUE_MAX];
    int fd,byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/period",pwmnum);
    fd=open(path,O_WRONLY);
    if(-1==fd){
        fprintf(stderr, "Failed to open in period!\n");
        return(-1);
    }

    byte=snprintf(s_values_str,10,"%d",value);

    if(-1==write(fd,s_values_str,byte)){
        fprintf(stderr, "Failed to write value in period!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(0);
}

static int PWMWriteDutyCycle(int pwmnum, int value){
    char path[VALUE_MAX];
    char s_values_str[VALUE_MAX];
    int fd,byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle",pwmnum);
    fd=open(path,O_WRONLY);
    if(-1==fd){
        fprintf(stderr,"Failed to oepn in duty_cycle!\n");
        return(-1);
    }

    byte=snprintf(s_values_str, 10,"%d",value);

    if(-1==write(fd,s_values_str,byte)){
        fprintf(stderr, "Failed to write value! in duty_cycle\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(0);
}

static int GPIOExport(int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (-1 == fd){
        fprintf(stderr, "Failed to open export for writing!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

static int GPIOUnexport(int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (-1 == fd){
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return (-1);
    }
    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

static int GPIODirection(int pin, int dir) 
{
    static const char s_directions_str[] = "in\0out";
    
    char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    fd = open(path, O_WRONLY);
    if (-1 == fd){
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return(-1);
    }

    if(-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)){
        fprintf(stderr, "Failed to set direction!\n");
        // close(fd);
        return(-1);
    }

    close(fd);
    return(0);
}

static int GPIORead(int pin)
{
    char path[VALUE_MAX];
    char value_str[3];
    int fd; 

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd){
        fprintf(stderr, "Failed to open gpio value for raeding!\n");
        return (-1);
    }

    if (-1 == read(fd, value_str, 3)){
        fprintf(stderr, "Failed to read value!\n");
        // close(fd);
        return(-1);
    }

    close(fd);
    return(atoi(value_str));
} 

static int GPIOWrite(int pin, int value)
{
    static const char s_values_str[] = "01";
    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd){
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return (-1);
    }

    if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)){
        fprintf(stderr, "Failed to write value!\n");
        // close(fd);
        return(-1);
    }

    close(fd);
    return(0);
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int main(int argc, char *argv[]) {
    int serv_sock = -1;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    char buffer[BUFFER_MAX];
    memset(buffer, 0, sizeof(buffer));
    fflush(stdout);
    fflush(stdin);
    int str_len;
    int received_total;

    int pwmnum = 0;  // 사용할 PWM 번호
    int period = 20000000;  // PWM 주기 (나노초 단위)

    if(-1 == GPIOExport(POUT)) return (1);
    if(-1 == GPIODirection(POUT,OUT)) return (2);
     // PWM 설정
    if (PWMExport(pwmnum) == -1) {
        printf("Failed to export PWM!\n");
        return -1;
    }

    if (PWMEnable(pwmnum) == -1) {
        printf("Failed to enable PWM!\n");
        return -1;
    }

    if (PWMWritePeriod(pwmnum, period) == -1) {
        printf("Failed to write PWM period!\n");
        return -1;
    }

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");
    
    printf("Binding....\n");
    while (1) {
        if (clnt_sock < 0) {
            clnt_addr_size = sizeof(clnt_addr);
            clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
            if (clnt_sock == -1)
                error_handling("accept() error");
        } 

        str_len = read(clnt_sock, ControlMessage, sizeof(ControlMessage));
        if (str_len == -1)
                error_handling("read() error");
        ControlMessage[str_len] = '\0';
        printf("ControlMessage: %s\n",ControlMessage);

        str_len = write(clnt_sock,"0",1);
        if (str_len == -1)
            error_handling("write() error"); // 잘 받았다고 보냄

        while ((str_len = read(clnt_sock, buffer, sizeof(buffer))) > 0) {
            if(str_len == 0) {
                error_handling("read() error");
            } 
            if(buffer[0] == '0') break;
        } // 잘 전달된 거 확인

        /*
            받은 message 분석 및 해당 값으로 PWM, LED 키기
            
        */
            int IsOn = atoi(ControlMessage);

            if(IsOn){
                    if(-1 == GPIOWrite(POUT, IsOn)) return(3);
                    if (PWMWriteDutyCycle(pwmnum, MOTOR_ON) == -1) {
                    printf("Failed to write PWM duty cycle!\n");
                    }
                    usleep(50000);
            } else {
                    printf("LED OFF\n");
                     if(-1 == GPIOWrite(POUT, IsOn)) return(3);
                    if (PWMWriteDutyCycle(pwmnum, MOTOR_OFF) == -1) {
                    printf("Failed to write PWM duty cycle!\n");
                    }
                    usleep(50000);
            }       
    }
    close(serv_sock);
    clnt_sock = -1;
    if(-1 == GPIOUnexport(POUT)) return (4);
    
    return 0;
}