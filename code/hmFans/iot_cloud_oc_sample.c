#include "E53_IA1.h"
#include "cmsis_os2.h"
#include "dbg.h"
#include "gui.h"
#include "hi_wifi_api.h"
#include "lwip/api_shell.h"
#include "lwip/ip_addr.h"  // SDK提供的wifi功能接口
#include "lwip/netifapi.h" //协议栈IP地址操作接口
#include "lwip/sockets.h"
#include "motor_module.h"
#include "oc_mqtt.h"
#include "ohos_init.h"
#include "oled.h"
#include "oled_module.h"
#include "test.h"
#include "wifi_connect.h"
#include "wifi_device.h"
#include "wifi_error_code.h"
#include "wifi_event.h"
#include "wifi_hotspot_config.h"
#include <base_type.h>
#include <hi_gpio.h>
#include <hi_io.h>
#include <hi_spi.h>
#include <hi_task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MSGQUEUE_OBJECTS 16 // number of Message Queue Objects

typedef struct
{ // object data type
    char *Buf;
    uint8_t Idx;
} MSGQUEUE_OBJ_t;

MSGQUEUE_OBJ_t msg;
osMessageQueueId_t mid_MsgQueue; // message queue id

#define WIFI_SSID "openhuaw"
#define WIFI_PASSWORD "sjxsjxsjx"

#define CLIENT_ID "64a76924ae80ef457fc068c0_202307sjx_0_0_2023070701"

#define USERNAME "64a76924ae80ef457fc068c0_202307sjx"

#define PASSWORD "cff6077a1602fb45ecc26b41988a653d94232be71b00c13fcb6035d491bc3a90"

typedef enum {
    en_msg_cmd = 0,
    en_msg_report,
} en_msg_type_t;

typedef struct
{
    char *request_id;
    char *payload;
} cmd_t;

typedef struct
{
    int lum;
    int temp;
    int hum;
} report_t;

typedef struct
{
    int t;
    int h;
} th;

static th thh;

typedef struct
{
    en_msg_type_t msg_type;
    union {
        cmd_t cmd;
        report_t report;
    } msg;
} app_msg_t;

typedef struct
{
    int connected;
    int led;
    int motor;
    int speed;
    int mode;
} app_cb_t;
static app_cb_t g_app_cb;

static void deal_report_msg(report_t *report)
{
    printf("data report\r\n");
    oc_mqtt_profile_service_t service;
    oc_mqtt_profile_kv_t temperature;
    oc_mqtt_profile_kv_t humidity;
    oc_mqtt_profile_kv_t luminance;
    oc_mqtt_profile_kv_t led;
    oc_mqtt_profile_kv_t motor;
    oc_mqtt_profile_kv_t mode;

    service.event_time = NULL;
    service.service_id = "Agriculture";
    service.service_property = &temperature;
    service.nxt = NULL;

    temperature.key = "Temperature";
    temperature.value = &report->temp;
    temperature.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    temperature.nxt = &humidity;

    humidity.key = "Humidity";
    humidity.value = &report->hum;
    humidity.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    humidity.nxt = &led;

    led.key = "LightStatus";
    led.value = g_app_cb.led ? "ON" : "OFF";
    led.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    led.nxt = &motor;

    motor.key = "MotorStatus";
    motor.value = g_app_cb.motor ? "ON" : "OFF";
    motor.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    motor.nxt = &mode;

    mode.key = "ModeStatus";
    mode.value = g_app_cb.mode ? "Auto" : "Manual";
    mode.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    mode.nxt = NULL;

    oc_mqtt_profile_propertyreport(USERNAME, &service);
    return;
}

void oc_cmd_rsp_cb(uint8_t *recv_data, size_t recv_size, uint8_t **resp_data, size_t *resp_size)
{
    app_msg_t *app_msg;

    int ret = 0;
    app_msg = malloc(sizeof(app_msg_t));
    app_msg->msg_type = en_msg_cmd;
    app_msg->msg.cmd.payload = (char *)recv_data;

    printf("recv data is %.*s\n", recv_size, recv_data);
    ret = osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U);
    if (ret != 0) {
        free(recv_data);
    }
    *resp_data = NULL;
    *resp_size = 0;
}

///< COMMAND DEAL
#include <cJSON.h>
static void deal_cmd_msg(cmd_t *cmd)
{
    cJSON *obj_root;
    cJSON *obj_cmdname;
    cJSON *obj_paras;
    cJSON *obj_para;

    int cmdret = 1;
    printf("cmd->payload %s \r\n", cmd->payload);
    oc_mqtt_profile_cmdresp_t cmdresp;
    obj_root = cJSON_Parse(cmd->payload);
    if (NULL == obj_root) {
        goto EXIT_JSONPARSE;
    }

    obj_cmdname = cJSON_GetObjectItem(obj_root, "command_name");
    if (NULL == obj_cmdname) {
        goto EXIT_CMDOBJ;
    }
    if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_Light")) {
        obj_paras = cJSON_GetObjectItem(obj_root, "paras");
        if (NULL == obj_paras) {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Light");
        if (NULL == obj_para) {
            goto EXIT_OBJPARA;
        }
        ///< operate the LED here
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON")) {
            g_app_cb.led = 1;
            Light_StatusSet(ON);
            printf("Light On!");
        }
        else {
            g_app_cb.led = 0;
            Light_StatusSet(OFF);
            printf("Light Off!");
        }
        cmdret = 0;
    }
    else if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_Motor")) {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (NULL == obj_paras) {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Motor");
        if (NULL == obj_para) {
            goto EXIT_OBJPARA;
        }
        ///< operate the Motor here
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON")) {
            g_app_cb.motor = 1;
            g_app_cb.speed = 1;
            Motor_StatusSet(ON);
            printf("Motor On!");
        }
        else {
            g_app_cb.motor = 0;
            g_app_cb.speed = 0;
            Motor_StatusSet(OFF);
            printf("Motor Off!");
        }
        cmdret = 0;
    }
    else if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Control_Motor_speed")) {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (NULL == obj_paras) {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Speed");
        if (NULL == obj_para) {
            goto EXIT_OBJPARA;
        }
        ///< operate the Motor here
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "1")) {
            g_app_cb.speed = 1;
            g_app_cb.motor = 1;
            Speed_StatusSet(1);
            printf("Motor speed：1!");
        }
        else if (0 == strcmp(cJSON_GetStringValue(obj_para), "2")) {
            g_app_cb.speed = 2;
            g_app_cb.motor = 1;
            Speed_StatusSet(2);
            printf("Motor speed：2!");
        }
        else if (0 == strcmp(cJSON_GetStringValue(obj_para), "3")) {
            g_app_cb.speed = 3;
            g_app_cb.motor = 1;
            Speed_StatusSet(3);
            printf("Motor speed：3!");
        }
        cmdret = 0;
    }
    else if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_Mode")) {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (NULL == obj_paras) {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Mode");
        if (NULL == obj_para) {
            goto EXIT_OBJPARA;
        }
        ///< operate the Motor here
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "Auto")) {
            g_app_cb.mode = 1;
            hi_gpio_set_ouput_val(HI_GPIO_IDX_7, HI_GPIO_VALUE1);
            printf("Mode Auto!");
        }
        else {
            g_app_cb.mode = 0;
            hi_gpio_set_ouput_val(HI_GPIO_IDX_7, HI_GPIO_VALUE0);
            printf("Mode Manual!");
        }
        cmdret = 0;
    }

EXIT_OBJPARA:
EXIT_OBJPARAS:
EXIT_CMDOBJ:
    cJSON_Delete(obj_root);
EXIT_JSONPARSE:
    ///< do the response
    cmdresp.paras = NULL;
    cmdresp.request_id = cmd->request_id;
    cmdresp.ret_code = cmdret;
    cmdresp.ret_name = NULL;
    (void)oc_mqtt_profile_cmdresp(NULL, &cmdresp);
    return;
}

static int task_main_entry(void)
{
    app_msg_t *app_msg;

    E53_IA1_Init();
    OLED_Init();   // 初始化OLED
    OLED_Clear(0); // 清屏（全黑）

    // connect to wifi
    uint32_t ret = WifiConnect(WIFI_SSID, WIFI_PASSWORD);
    if (ret != 0) {
        printf("WifiConnect failed \r\n");
    }

    // connect to huawei_cloud
    device_info_init(CLIENT_ID, USERNAME, PASSWORD);
    oc_mqtt_init();
    oc_set_cmd_rsp_cb(oc_cmd_rsp_cb);

    // motor_gpio_io_init();
    while (1) {
        app_msg = NULL;
        (void)osMessageQueueGet(mid_MsgQueue, (void **)&app_msg, NULL, 0U);
        if (NULL != app_msg) {
            printf("mqtt msg process\r\n");
            switch (app_msg->msg_type) {
            case en_msg_cmd:
                deal_cmd_msg(&app_msg->msg.cmd);
                break;
            case en_msg_report:
                deal_report_msg(&app_msg->msg.report);
                break;
            default:
                break;
            }
            free(app_msg);
        }
    }
    return 0;
}

hi_void gpio_getval_auto(hi_void)
{
    hi_u32 ret;
    int temp;
    static int key = -1;
    hi_gpio_value gpio_val_1 = HI_GPIO_VALUE1;
    ret = hi_gpio_get_input_val(HI_GPIO_IDX_5, &gpio_val_1);
    if (ret != HI_ERR_SUCCESS) {
        printf("===== ERROR ===== gpio -> hi_gpio_get_input_val ret:%d\r\n", ret);
        return;
    }

    // printf("----- gpio input val is:%d. -----\r\n", gpio_val_1);
    if (gpio_val_1 == 0) {
        if (gpio_val_1 == 0) {
            key++;
        }

        switch (key) {

        case 0:
            thh.t = 10;
            thh.h = 20;
            g_app_cb.speed = 1;
            break;

        case 1:
            thh.t = 20;
            thh.h = 30;
            g_app_cb.speed = 2;
            break;

        case 2:
            thh.t = 40;
            thh.h = 50;
            g_app_cb.speed = 3;
            break;

        default:
            printf("invalid mode \r\n");
            break;
        }

        if (key >= 3) {
            key = -1;
        }
    }
}

int gpio_getval1(int s)
{
    hi_u32 ret;
    // int temp;
    static int key = 0;
    if (s != 0) {
        key = s;
    }
    else{
        key = 0;
    }
    hi_gpio_value gpio_val_1 = HI_GPIO_VALUE1;

    // temp = infrared_ctrl_t();
    ret = hi_gpio_get_input_val(HI_GPIO_IDX_5, &gpio_val_1);
    if (ret != HI_ERR_SUCCESS) {
        printf("===== ERROR ===== gpio -> hi_gpio_get_input_val ret:%d\r\n", ret);
        return key;
    }

    // printf("----- gpio input val is:%d. -----\r\n", gpio_val_1);

    if (gpio_val_1 == 0) {
        if (gpio_val_1 == 0) {
            key++;
        }

        switch (key) {
        case 0:
            break;

        case 1:
            motor_pwm_start(1);
            break;

        case 2:
            motor_pwm_start(2);
            break;

        case 3:
            motor_pwm_start(3);
            break;

        default:
            printf("invalid mode \r\n");
        }

        if (key >= 4 || key == 0) {
            key = 0;
            ret = hi_pwm_stop(HI_PWM_PORT_PWM2);
            if (ret != 0) {
                printf("hi_pwm_stop failed \r\n");
            }
        }
    }
    return key;
    // printf("key : %d \r\n",key);
}

static unsigned int g_MonitorTask;

hi_void oled_gpio_io_init(void)
{
    hi_u32 ret;

    // 连接OLED屏幕的CS引脚，负责使能OLED屏幕
    ret = hi_io_set_func(HI_IO_NAME_GPIO_8, HI_IO_FUNC_GPIO_8_GPIO);
    if (ret != HI_ERR_SUCCESS) {
        printf("===== ERROR ===== gpio -> hi_io_set_func ret:%d\r\n", ret);
        return;
    }
    printf("----- io set func success-----\r\n");

    ret = hi_gpio_set_dir(HI_GPIO_IDX_8, HI_GPIO_DIR_OUT);
    if (ret != HI_ERR_SUCCESS) {
        printf("===== ERROR ===== gpio -> hi_gpio_set_dir1 ret:%d\r\n", ret);
        return;
    }
    printf("----- gpio set dir success! -----\r\n");

    // 连接OLED屏幕的DC引脚，负责对OLED屏幕进行指令控制
    ret = hi_io_set_func(HI_IO_NAME_GPIO_11, HI_IO_FUNC_GPIO_11_GPIO);
    if (ret != HI_ERR_SUCCESS) {
        printf("===== ERROR ===== gpio -> hi_io_set_func ret:%d\r\n", ret);
        return;
    }
    printf("----- io set func success-----\r\n");

    ret = hi_gpio_set_dir(HI_GPIO_IDX_11, HI_GPIO_DIR_OUT);
    if (ret != HI_ERR_SUCCESS) {
        printf("===== ERROR ===== gpio -> hi_gpio_set_dir1 ret:%d\r\n", ret);
        return;
    }
    printf("----- gpio set dir success! -----\r\n");
}

static int task_sensor_entry(void)
{
    app_msg_t *app_msg;
    E53_IA1_Data_TypeDef data;
    // E53_IA1_Init();
    // OLED_Init();   // 初始化OLED
    // OLED_Clear(0); // 清屏（全黑）

    while (1) {

        E53_IA1_Read_Data(&data); // 此处读取温度湿度数据

        hi_watchdog_feed(); // feed dog 防止死机
        app_msg = malloc(sizeof(app_msg_t));

        if (NULL != app_msg) { // 将数据放入消息队列
            app_msg->msg_type = en_msg_report;
            app_msg->msg.report.hum = (int)data.Humidity;
            app_msg->msg.report.temp = (int)data.Temperature;
            if (0 != osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U)) {
                free(app_msg);
            }
        }

        if (g_app_cb.mode == 1) {
            gpio_getval_auto();
            if (data.Humidity > thh.h || data.Temperature > thh.t) {
                Speed_StatusSet(g_app_cb.speed);
                printf("speed is %d\n", g_app_cb.speed);
            }
            else {
                hi_pwm_stop(HI_PWM_PORT_PWM2);
            }
        }

        else if (g_app_cb.mode == 0) {
            g_app_cb.speed = gpio_getval1(g_app_cb.speed);
            printf("speed is %d\n", g_app_cb.speed);
        }

        printf("data en queue\r\n"); // 打印数据入队列

        printf("before main page clear \r\n");

        int speed01 = g_app_cb.speed;
        int motor01 = g_app_cb.motor;
        int mode01 = g_app_cb.mode;
        TEST_tempAndHumMenu(speed01, motor01, mode01);

        // OLED_Clear(0);
    }
    return 0;
}

#define AP_SSID "boradOP"
#define AP_PSK "sjxsjxsjx"

#define ONE_SECOND 1
#define DEF_TIMEOUT 15

static void OnHotspotStaJoinHandler(StationInfo *info);
static void OnHotspotStateChangedHandler(int state);
static void OnHotspotStaLeaveHandler(StationInfo *info);

static struct netif *g_lwip_netif = NULL;
static int g_apEnableSuccess = 0;
static WifiEvent g_wifiEventHandler = {0};
WifiErrorCode error1;

static BOOL WifiAPTask(void)
{
    // 延时2S便于查看日志
    osDelay(200);

    // 注册wifi事件的回调函数
    g_wifiEventHandler.OnHotspotStaJoin = OnHotspotStaJoinHandler;
    g_wifiEventHandler.OnHotspotStaLeave = OnHotspotStaLeaveHandler;
    g_wifiEventHandler.OnHotspotStateChanged = OnHotspotStateChangedHandler;
    error1 = RegisterWifiEvent(&g_wifiEventHandler);
    if (error1 != WIFI_SUCCESS) {
        printf("RegisterWifiEvent failed, error = %d.\r\n", error1);
        return -1;
    }
    printf("RegisterWifiEvent succeed!\r\n");
    // 检查热点模式是否使能
    if (IsHotspotActive() == WIFI_HOTSPOT_ACTIVE) {
        printf("Wifi station is  actived.\r\n");
        return -1;
    }
    // 设置指定的热点配置
    HotspotConfig config = {0};

    strcpy(config.ssid, AP_SSID);
    strcpy(config.preSharedKey, AP_PSK);
    config.securityType = WIFI_SEC_TYPE_PSK;
    config.band = HOTSPOT_BAND_TYPE_2G;
    config.channelNum = 7;

    error1 = SetHotspotConfig(&config);
    if (error1 != WIFI_SUCCESS) {
        printf("SetHotspotConfig failed, error = %d.\r\n", error1);
        return -1;
    }
    printf("SetHotspotConfig succeed!\r\n");

    // 启动wifi热点模式
    error1 = EnableHotspot();
    if (error1 != WIFI_SUCCESS) {
        printf("EnableHotspot failed, error = %d.\r\n", error1);
        return -1;
    }
    printf("EnableHotspot succeed!\r\n");

    /****************以下为UDP服务器代码,默认IP:192.168.5.1***************/
    // 在sock_fd 进行监听
    int sock_fd;
    // 服务端地址信息
    struct sockaddr_in server_sock;

    // 创建socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket is error.\r\n");
        return -1;
    }

    bzero(&server_sock, sizeof(server_sock));
    server_sock.sin_family = AF_INET;
    server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock.sin_port = htons(8888);

    // 调用bind函数绑定socket和地址
    if (bind(sock_fd, (struct sockaddr *)&server_sock, sizeof(struct sockaddr)) == -1) {
        perror("bind is error.\r\n");
        return -1;
    }

    int ret;
    char recvBuf[512] = {0};
    // 客户端地址信息
    struct sockaddr_in client_addr;
    int size_client_addr = sizeof(struct sockaddr_in);
    while (1) {

        printf("Waiting to receive data...\r\n");
        memset(recvBuf, 0, sizeof(recvBuf));
        ret = recvfrom(sock_fd, recvBuf, sizeof(recvBuf), 0, (struct sockaddr *)&client_addr, (socklen_t *)&size_client_addr);
        if (ret < 0) {
            printf("UDP server receive failed!\r\n");
            return -1;
        }
        printf("receive %d bytes of data from ipaddr = %s, port = %d.\r\n", ret, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("data is %s\r\n", recvBuf);
        ret = sendto(sock_fd, recvBuf, strlen(recvBuf), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (ret < 0) {
            printf("UDP server send failed!\r\n");
            return -1;
        }
        if (0 == strcmp(recvBuf, "s1")) {
            g_app_cb.speed = 1;
            g_app_cb.motor = 1;
            Speed_StatusSet(1);
            printf("Motor speed：1!");
        }
        else if (0 == strcmp(recvBuf, "s2")) {
            g_app_cb.speed = 2;
            g_app_cb.motor = 1;
            Speed_StatusSet(2);
            printf("Motor speed：2!");
        }
        else if (0 == strcmp(recvBuf, "s3")) {
            g_app_cb.speed = 3;
            g_app_cb.motor = 1;
            Speed_StatusSet(3);
            printf("Motor speed：3!");
        }
        else if (0 == strcmp(recvBuf, "ON")) {
            g_app_cb.speed = 1;
            g_app_cb.motor = 1;
            Motor_StatusSet(ON);
            printf("Motor ON!");
        }
        else if (0 == strcmp(recvBuf, "OFF")) {
            g_app_cb.speed = 0;
            g_app_cb.motor = 0;
            Motor_StatusSet(OFF);
            printf("Motor OFF!");
        }
        else {
            printf("Not Satisfiable!\r\n");
        }
    }
    /*********************END********************/
}

static void OnHotspotStaJoinHandler(StationInfo *info)
{
    if (info == NULL) {
        printf("HotspotStaJoin:info is null.\r\n");
    }
    else {
        static char macAddress[32] = {0};
        unsigned char *mac = info->macAddress;
        snprintf(macAddress, sizeof(macAddress), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        printf("HotspotStaJoin: macAddress=%s, reason=%d.\r\n", macAddress, info->disconnectedReason);
        g_apEnableSuccess++;
    }
    return;
}

static void OnHotspotStaLeaveHandler(StationInfo *info)
{
    if (info == NULL) {
        printf("HotspotStaLeave:info is null.\r\n");
    }
    else {
        static char macAddress[32] = {0};
        unsigned char *mac = info->macAddress;
        snprintf(macAddress, sizeof(macAddress), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        printf("HotspotStaLeave: macAddress=%s, reason=%d.\r\n", macAddress, info->disconnectedReason);
        g_apEnableSuccess--;
    }
    return;
}

static void OnHotspotStateChangedHandler(int state)
{
    printf("HotspotStateChanged:state is %d.\r\n", state);
    if (state == WIFI_HOTSPOT_ACTIVE) {
        printf("wifi hotspot active.\r\n");
    }
    else {
        printf("wifi hotspot noactive.\r\n");
    }
}
// int ledInitSign = 0;
static void OC_Demo(void)
{
    mid_MsgQueue = osMessageQueueNew(MSGQUEUE_OBJECTS, 10, NULL);
    if (mid_MsgQueue == NULL) {
        printf("Falied to create Message Queue!\n");
    }


    // main task
    osThreadAttr_t attr;
    attr.name = "task_main_entry";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 10240;
    attr.priority = 24;

    if (osThreadNew((osThreadFunc_t)task_main_entry, NULL, &attr) == NULL) {
        printf("Falied to create task_main_entry!\n");
    }

    // sensor task

    oled_gpio_io_init();
    hi_spi_deinit(HI_SPI_ID_0);
    screen_spi_master_init(0);

    osThreadAttr_t attr_sensor;
    attr_sensor.name = "task_sensor_entry";
    attr_sensor.attr_bits = 0U;
    attr_sensor.cb_mem = NULL;
    attr_sensor.cb_size = 0U;
    attr_sensor.stack_mem = NULL;
    attr_sensor.stack_size = 4096;
    attr_sensor.priority = 24; //

    if (osThreadNew((osThreadFunc_t)task_sensor_entry, NULL, &attr_sensor) == NULL) {
        printf("Falied to create task_sensor_entry!\n");
    }

    //socket
    
    osThreadAttr_t attr1;

    attr1.name = "WifiAPTask";
    attr1.attr_bits = 0U;
    attr1.cb_mem = NULL;
    attr1.cb_size = 0U;
    attr1.stack_mem = NULL;
    attr1.stack_size = 10240;
    attr1.priority = 25;

    if (osThreadNew((osThreadFunc_t)WifiAPTask, NULL, &attr1) == NULL)
    {
        printf("Falied to create WifiAPTask!\r\n");
    }
}

APP_FEATURE_INIT(OC_Demo);
// SYS_RUN(OC_Demo);
