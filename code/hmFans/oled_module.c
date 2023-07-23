#include <stdio.h>
#include <unistd.h>
#include <hi_task.h>
#include <hi_gpio.h>
#include <hi_spi.h>
#include <base_type.h>
#include <hi_io.h>
#include "oled_module.h"
#include "dbg.h"
#include "ohos_init.h"
#include "oled.h"
#include "gui.h"
#include "test.h"

// void test_led_screen(void)
// {
//     printf("before oled clear \r\n");
//     OLED_Init();               //初始化OLED  
//     OLED_Clear(0);             //清屏（全黑）
//     printf("after oled clear \r\n");
    
//     while(1)
//     {
//         printf("before main page clear \r\n");
//         /*
//         TEST_BMP();              //BMP单色图片显示测试
//         TEST_LINE();             //绘制线条测试
//         TEST_Chinese();          //中文显示测试
//         printf("test dbg %s,%s,%d \r\n",__FILE__,__func__,__LINE__);
//         OLED_Clear(0); 
//        */
//         // TEST_BMP();  
//         TEST_Menu2();            //菜单2显示测试
//         OLED_Clear(0);
//      }
// }

// hi_void oled_gpio_io_init(void)
// {
//     hi_u32 ret;
   
//     //连接OLED屏幕的CS引脚，负责使能OLED屏幕 
//     ret = hi_io_set_func(HI_IO_NAME_GPIO_8, HI_IO_FUNC_GPIO_8_GPIO);
//     if (ret != HI_ERR_SUCCESS) {
//         printf("===== ERROR ===== gpio -> hi_io_set_func ret:%d\r\n", ret);
//         return;
//     }
//     printf("----- io set func success-----\r\n");
	
//     ret = hi_gpio_set_dir(HI_GPIO_IDX_8, HI_GPIO_DIR_OUT);
//     if (ret != HI_ERR_SUCCESS) {
//         printf("===== ERROR ===== gpio -> hi_gpio_set_dir1 ret:%d\r\n", ret);
//         return;
//     }
//     printf("----- gpio set dir success! -----\r\n");

//     //连接OLED屏幕的DC引脚，负责对OLED屏幕进行指令控制
//     ret = hi_io_set_func(HI_IO_NAME_GPIO_11, HI_IO_FUNC_GPIO_11_GPIO);
//     if (ret != HI_ERR_SUCCESS) {
//         printf("===== ERROR ===== gpio -> hi_io_set_func ret:%d\r\n", ret);
//         return;
//     }
//     printf("----- io set func success-----\r\n");
   
//     ret = hi_gpio_set_dir(HI_GPIO_IDX_11, HI_GPIO_DIR_OUT);
//     if (ret != HI_ERR_SUCCESS) {
//         printf("===== ERROR ===== gpio -> hi_gpio_set_dir1 ret:%d\r\n", ret);
//         return;
//     }
//     printf("----- gpio set dir success! -----\r\n");

// }

// static unsigned int g_MonitorTask;
// const hi_task_attr MonitorTaskAttr_oled = {
//     .task_prio = 20,
//     .stack_size = 4096,
//     .task_name = "oled_demo_task",
// };


// static int oled_demo(hi_void)
// {    
//     app_msg_t *app_msg;

//     int ret;
//     oled_gpio_io_init();
//     hi_spi_deinit(HI_SPI_ID_0);
//     screen_spi_master_init(0);

//     ret = hi_task_create(&g_MonitorTask, // task标识 //
//         &MonitorTaskAttr_oled,
//         test_led_screen, // task处理函数 //
//         NULL); // task处理函数参数 //

//     if (ret < 0) {
//         printf("Create monitor task failed [%d]\r\n", ret);
//         return 0;
//     }
// }

// APP_FEATURE_INIT(oled_demo);
