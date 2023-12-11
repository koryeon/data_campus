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
#include <jansson.h>
#include <pthread.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <wiringPi.h>

#define BUFFER_MAX 3
#define DIRECTION_MAX 45

#define IN 0
#define OUT 1
#define PWM 0

#define LOW 0
#define HIGH 1
#define VALUE_MAX 256

#define POUT 23
#define PIN 18
#define RAIN_PIN 5

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static const char *DEVICE = "/dev/spidev0.0";
static uint8_t MODE = 0;
static uint8_t BITS = 0;
static uint32_t CLOCK = 100000;
static uint16_t DELAY = 5;

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

void *execute_python_cd(void *arg){
    system("python /home/pi/test.py");
    pthread_exit(NULL);
}
void *execute_DHT11_cd(void *arg){
    system("gcc -o DHT11 DHT11.c -lwiringPi -ljansson");
    system("./DHT11");
    pthread_exit(NULL);
}

static int prepare(int fd){
    if(ioctl(fd,SPI_IOC_WR_MODE, &MODE)== -1){
        perror("Can't set MODE");
        return -1;
    }
    if(ioctl(fd,SPI_IOC_WR_BITS_PER_WORD, &BITS)== -1){
        perror("Can't set number of BITS");
        return -1;
    }
    if(ioctl(fd,SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK)== -1){
        perror("Can't set write CLOCK");
        return -1;
    }
    if(ioctl(fd,SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK)== -1){
        perror("Can't set read CLOCK");
        return -1;
    }

    return 0;
}

uint8_t control_bits_differential(uint8_t channel){
    return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel){
    return 0x8 | control_bits_differential(channel);
}

int readadc (int fd, uint8_t channel){
    uint8_t tx[] = {1, control_bits(channel),0};
    uint8_t rx[3];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word = BITS,
    };

    if(ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1){
        perror("IO Error");
        abort();
    }

    return ((rx[1]<<8) & 0x300) | (rx[2] & 0xFF);
}

int main(void){
    int fd = open(DEVICE, O_RDWR);
    if(fd<=0){
        perror("Device open error");
        return -1;
    }

    if(prepare(fd)==-1){
        perror("Device prepare error");
        return -1;
    }

    if(wiringPiSetup() == -1){
        printf("wiringPiSetup failed\n");
        return 1;
    }
    if(-1 == GPIOExport(PIN))
        return(1);
    if(-1 == GPIODirection(PIN, IN))
        return(2);
    typedef struct {
        int Humid;
        int Light;
        int Rain;
        int Temp;
        int Trash;
    } SensorData;
    pthread_t python_thread;
    pthread_t DHT11_thread;
    int i = 1;

    while(1){
        
        int light = readadc(fd,0);
        printf("light value : %d\n",light);
        int rain = 0;
        int rainValue = GPIORead(RAIN_PIN);
        if (rainValue == LOW) {
            printf("Rain Detected\n");
            rain = 1;
        } else {
            printf("No Rain\n");
        }
        int thread_create_result_1 = pthread_create(&DHT11_thread, NULL, execute_DHT11_cd, NULL);
        if(thread_create_result_1 != 0){
            printf("Failed to create DHT11 thread\n");
            return 1;
        }
        int thread_join_result_1 = pthread_join(DHT11_thread, NULL);
        if(thread_join_result_1 != 0){
            printf("Failed to join DHT11 thread\n");
            return 1;
        }

        FILE *fp = fopen("DHT_data.json", "r");
        if(fp == NULL){
            printf("Failed to open the JSON file.\n");
            return 1;
        }

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 파일 내용을 저장할 버퍼 할당
    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        printf("Failed to allocate memory for the JSON buffer.\n");
        fclose(fp);
        return 1;
    }

    // 파일 내용 읽기
    fread(buffer, 1, file_size, fp);
    buffer[file_size] = '\0';

    // JSON 파싱
    json_error_t error;
    json_t *root_1 = json_loads(buffer, 0, &error);
    if (root_1 == NULL) {
        printf("Failed to parse the JSON data. Error on line %d: %s\n", error.line, error.text);
        free(buffer);
        fclose(fp);
        return 1;
    }

    // "Humid" 필드 값 가져오기
    json_t *humid_value = json_object_get(root_1, "Humid");
    if (!json_is_integer(humid_value)) {
        printf("Invalid value for 'Humid'.\n");
        json_decref(root_1);
        free(buffer);
        fclose(fp);
        return 1;
    }
    int humid_json = json_integer_value(humid_value);

    // "Temperature" 필드 값 가져오기
    json_t *temp_value = json_object_get(root_1, "Temperature");
    if (!json_is_integer(temp_value)) {
        printf("Invalid value for 'Temperature'.\n");
        json_decref(root_1);
        free(buffer);
        fclose(fp);
        return 1;
    }
    int temperature_json = json_integer_value(temp_value);
    json_decref(root_1);
    free(buffer);
    fclose(fp);
        /*
        여기서 센서값 읽어오기
        */
        //이후 값 정리하기
        sleep(5);

        SensorData data;
        data.Humid = humid_json;
        data.Light = light;
        data.Rain = rain;
        data.Temp = temperature_json;
        data.Trash = i;

        // JSON 객체 생성
        json_t *root = json_object();
        json_object_set_new(root, "Humid", json_integer(data.Humid));
        json_object_set_new(root, "Light", json_integer(data.Light));
        json_object_set_new(root, "Rain", json_integer(data.Rain));
        json_object_set_new(root, "Temp", json_integer(data.Temp));
        json_object_set_new(root, "Trash", json_integer(data.Trash));
        // JSON 문자열로 변환
        char *jsonStr = json_dumps(root, JSON_INDENT(4));

        // JSON 출력
        printf("%s\n", jsonStr);

        // JSON 파일로 저장
        FILE *file = fopen("sensor_data.json", "w");
        if (file == NULL) {
            fprintf(stderr, "파일을 열 수 없습니다.\n");
            return 1;
        }
        fprintf(file, "%s\n", jsonStr);
        fclose(file);
        // 메모리 해제
        json_decref(root);
        free(jsonStr);

        int thread_create_result = pthread_create(&python_thread, NULL, execute_python_cd, NULL);
        if(thread_create_result != 0){
            printf("Failed to create Python thread\n");
            return 1;
        }

        int thread_join_result = pthread_join(python_thread, NULL);
        if(thread_join_result != 0){
            printf("Failed to join Python thread\n");
            return 1;
        }
        printf("process cycle %d done\n", i++);
        printf("\n===============================================\n");
    }
    if(-1 == GPIOUnexport(PIN))
        return(4);
    return 0;
}