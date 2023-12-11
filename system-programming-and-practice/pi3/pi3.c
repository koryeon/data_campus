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
#include <jansson.h> // Json 파일에 대한 라이브러리로 추가적인 설치가 필요합니다.
#include <arpa/inet.h>
#include <sys/socket.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <lcd.h>

#define LCD_ADDR 0x3f // LCD의 I2C 주소
#define LCD_ROWS 2    // LCD 행 수
#define LCD_COLS 16   // LCD 열 수

// ------------- LCD -------------

#define BUFFER_MAX 3
#define DIRECTION_MAX 45
#define MAX_OUTPUT_LENGTH 256

#define SERVO_ON    350000
#define SERVO_OFF   2500000

#define PHTCriticalPoint 340
#define IN 0
#define OUT 1 
#define Button 21

#define LOW 0
#define HIGH 1
#define VALUE_MAX 256

/*
    Json 관련 struct
*/
typedef struct {
    int Humid;
    int Light;
    int Rain;
    int Temp;
} SensorData;

int fd;
int BLEN=1;

int LcdHumid = 1;
int LcdLight = 1;
int LcdRain = 1;
int LcdTemp = 1;

int pwmnum = 0;  // 사용할 PWM 번호
int period = 20000000;  // PWM 주기 (나노초 단위)

int toggle = 0; // button toggle 두개의 firebase를 접근하기 위해서
/*
    LCD 관련 함수들
*/
void write_word(int data){
    int temp = data;
    if ( BLEN == 1 )
        temp |= 0x08;
    else
        temp &= 0xF7;
    wiringPiI2CWrite(fd, temp);
}

void send_command(int comm){
    int buf;
    // Send bit7-4 firstly
    buf = comm & 0xF0;
    buf |= 0x04;                    // RS = 0, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);

    // Send bit3-0 secondly
    buf = (comm & 0x0F) << 4;
    buf |= 0x04;                    // RS = 0, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);
}

void send_data(int data){
    int buf;
    // Send bit7-4 firstly
    buf = data & 0xF0;
    buf |= 0x05;                    // RS = 1, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);

    // Send bit3-0 secondly
    buf = (data & 0x0F) << 4;
    buf |= 0x05;                    // RS = 1, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);
}

void init(){
    send_command(0x33);     // Must initialize to 8-line mode at first
    delay(5);
    send_command(0x32);     // Then initialize to 4-line mode
    delay(5);
    send_command(0x28);     // 2 Lines & 5*7 dots
    delay(5);
    send_command(0x0C);     // Enable display without cursor
    delay(5);
    send_command(0x01);     // Clear Screen
    wiringPiI2CWrite(fd, 0x08);
}

void clear(){
    send_command(0x01);     //clear Screen
}

void write_l(int x, int y, char data[]){
    int addr, i;
    int tmp;
    if (x < 0)  x = 0;
    if (x > 15) x = 15;
    if (y < 0)  y = 0;
    if (y > 1)  y = 1;

    // Move cursor
    addr = 0x80 + 0x40 * y + x;
    send_command(addr);

    tmp = strlen(data);
    for (i = 0; i < tmp; i++){
        send_data(data[i]);
    }
}

/*
    PWM 관련 함수들
*/
static int PWMExport(int pwmnum){
    #define BUFFER_MAX 3
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

/*
    GPIO 관련 함수들
*/
static int GPIOExport(int pin){
#define BUFFER_MAX 3
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if(-1 == fd) {
        fprintf(stderr, "Failed to open export for writing!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd,buffer,bytes_written);
    close(fd);
    return (0);
}

static int GPIOUnexport(int pin){
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport",O_WRONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer,BUFFER_MAX,"%d",pin);
    write(fd,buffer,bytes_written);
    close(fd);
    return(0);
}

static int GPIODirection(int pin, int dir){
    static const char s_directions_str[] = "in\nout";

    char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    fd = open(path, O_WRONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return(-1);
    }

    if(-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)){
        fprintf(stderr, "Failed to set direction!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return (0);
}

static int GPIORead(int pin){
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value",pin);
    fd = open(path,O_RDONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return(-1);
    }

    if(-1 == read(fd, value_str,3)){
        fprintf(stderr, "Failed to read value!\n");
        close(fd);
        return(-1);
    }

    close(fd);

    return (atoi(value_str));
}

/*
    Thread 관련 함수들
*/

void *execute_python(void *arg) {
    int result;
    if(toggle){
        result = system("python3 ./fileReadAnother.py");  
        printf("Resion0 result\n");
    }else{
        result = system("python3 ./fileRead.py");   
         printf("Resion1 result\n");
    }
    if (result == -1) {
            printf("Failed to execute Python process\n");
    }
    return NULL;
} // python 실행

void* lcdThreadFunction(void* arg) {
    while (1) {
        char string[10];

        sprintf(string, "hmd:%d%%", LcdHumid);
        write_l(0, 0, string);
        sprintf(string, "rain:%c", (LcdRain % 2 == 0) ? 'X' : 'O');
        write_l(9, 0, string);
        sprintf(string, "tmp:%dC", LcdTemp);
        write_l(0, 1, string);
        sprintf(string, "pht:%d", LcdLight );
        write_l(9, 1, string);

        usleep(1000000); // Delay for 1 second
    }
} // Lcd display

void *ServoMotorThreadFunction(void *arg){
    while(1){
        if( LcdLight >= PHTCriticalPoint)    
         {
            if (PWMWriteDutyCycle(pwmnum, SERVO_ON) == -1) {
                printf("Failed to write PWM duty cycle!\n");
                break;
            }
         }
        else {
             if (PWMWriteDutyCycle(pwmnum, SERVO_OFF) == -1) {
                printf("Failed to write PWM duty cycle!\n");
                break;
            }
        }
        
        usleep(1000000); // Delay for 1 second
    }
} // servo motor control

void* buttonThreadFunction(void* arg) {
    while(1)
    {  
        if(GPIORead(Button) == 1){
            printf("GPIORead : Button on\n");
            toggle = !toggle;
            printf("Toggle set %d\n",toggle);
        }
        usleep(100000);
    }
        
    if(-1 == GPIOUnexport(Button)) return NULL;
    return NULL;
} // button read

/*
    Error 관련 함수
*/

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int main(int argc, char *argv[]) {
    int duty_cycle = 1500000;  // PWM 듀티 사이클 (나노초 단위)
    
    // 각 Thread 선언
    pthread_t python_thread;
    pthread_t lcdThread; 
    pthread_t servoThread;
    pthread_t buttonThread;

    SensorData data;
    
    // Socket 통신
    int sock;
    struct sockaddr_in serv_addr;
    
    fflush(stdout);
    fflush(stdin);
    char msg[BUFFER_MAX];
    memset(msg, 0, sizeof(msg));

    char filename[VALUE_MAX];
    int str_len;
    int read_len, sent_len, sent_total;
    
    // LCD 관련 설정
    if (wiringPiSetup() == -1)
    return 1;
        
    fd = wiringPiI2CSetup(LCD_ADDR);
    init();
    
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

    // Thread 실행
    if (pthread_create(&servoThread, NULL, ServoMotorThreadFunction, &pwmnum) != 0) {
        printf("Failed to create servo control thread!\n");
        return -1;
    }
    if (pthread_create(&buttonThread, NULL, buttonThreadFunction, NULL) != 0) {
        printf("Failed to create button control thread!\n");
        return -1;
    }
    if (pthread_create(&lcdThread, NULL, lcdThreadFunction, NULL) != 0){
        printf("Failed to create lcd control thread!\n");
        return -1;
    }
    
    if (argc != 3) {
        printf("Usage : %s <IP> <port> \n", argv[0]);
        exit(1);
    }
    
    // Sock 연결
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    while(1){ // Main loop 시작
        int thread_create_result = pthread_create(&python_thread, NULL, execute_python, NULL); // python 실행을 위한 thread 실행
        if (thread_create_result != 0) {
            printf("Failed to create Python thread\n");
            return 1;
        }
        
        pthread_join(python_thread, NULL);
        const char* file_path  = "data.json";
        // 파일 열기
        FILE* file = fopen(file_path, "r");
        if (!file) {
            printf("Failed to open the file: %s\n", file_path);
            return 1;
        }
        
        // 파일 크기 확인
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // 파일 내용 읽기
        char* json_content = malloc(file_size + 1);
        fread(json_content, 1, file_size, file);
        json_content[file_size] = '\0';

        // 파일 닫기
        fclose(file);

        json_error_t error;
        json_t* root = json_loads(json_content, 0, &error);
        free(json_content);

        if (!root) {
            printf("Failed to parse the JSON: %s\n", error.text);
            return 1;
        }
        
        // Json 파일을 읽어서 firebase에서 읽어본 값 unpack
        json_unpack(root, "{s:i, s:i, s:i, s:i}",
                    "Humid", &(data.Humid),
                    "Light", &(data.Light),
                    "Rain", &(data.Rain),
                    "Temp", &(data.Temp));

        LcdHumid = data.Humid;
        LcdLight = data.Light;
        LcdRain = data.Rain;
        LcdTemp = data.Temp;

        /*
            Thread로 PWM으로 모터 제어 (커튼)
        */
        
        printf("Humid: %d\n", LcdHumid);
        printf("Light: %d\n", LcdLight);
        printf("Rain: %d\n", LcdRain);
        printf("Temp: %d\n", LcdTemp);
        printf("--------------------------------\n");
    
        memset(msg, 0, sizeof(msg));

        sprintf(msg, "%d",LcdRain);
        str_len = write(sock, msg, strlen(msg));
        if (str_len == -1)
            error_handling("write() error"); // 보냄

        while ((str_len = read(sock, msg, sizeof(msg))) > 0) {
                if(str_len == 0) {
                    error_handling("read() error");
                }
                if(msg[0] == '0') break;
                printf("waiting..\n");
        } // 잘 받았는지 확인

        str_len = write(sock, "0", 1);
        if (str_len == -1)
            error_handling("write() error"); // 잘 받았다고 보냄

        // 메모리 해제
        json_decref(root);
        // waiting...
        usleep(1000 * 1000);
    }

    // Thread join
    pthread_join(servoThread, NULL);
    pthread_join(lcdThread, NULL);
    pthread_join(buttonThread, NULL);

    return 0;
}
