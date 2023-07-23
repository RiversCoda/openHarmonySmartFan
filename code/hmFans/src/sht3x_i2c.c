#include <stdio.h>
#include <unistd.h>
#include "hi_timer.h"
#include "hi_gpio.h"
#include "dbg.h"
#include "sht3x_i2c.h"

float Temperature_t = 0;
float Humidity_t = 0;

static void dump_buf(unsigned char * buf,unsigned int len)
{
    if(buf == NULL)
        return ;

    DBG("in buf : \r\n");
    for(int i = 0; i < len; i++){
        printf("%#x ",*buf++); 
    }
    printf(" \r\n");
}

static unsigned char SHT3X_CalcCrc(unsigned char data[], unsigned int nbrOfBytes) 
{
    unsigned char bit; // bit mask
    unsigned char crc = 0xFF; // calculated checksum
    unsigned char byteCtr; // byte counter

    // calculates 8-Bit checksum with given polynomial
    for(byteCtr = 0; byteCtr < nbrOfBytes; byteCtr++)
    {
        crc ^= (data[byteCtr]);
        for(bit = 8; bit > 0; --bit)
        {
            if(crc & 0x80) 
                crc = (crc << 1) ^ POLYNOMIAL;
            else 
                crc = (crc << 1);
        }
    }

    return crc; 
}

static etError SHT3X_CheckCrc(unsigned char data[],\
    unsigned int nbrOfBytes, unsigned char checksum) 
{
    unsigned char crc; // calculated checksum

    // calculates 8-Bit checksum
    crc = SHT3X_CalcCrc(data, nbrOfBytes);

    // verify checksum
    if(crc != checksum) 
        return CHECKSUM_ERROR;
    else 
        return NO_ERROR; 
}

static int SHT3x_ReadSerialNumber(unsigned int *serialNumber)
{
    int ret = -1;
    unsigned char serialNumWords[6] = {0};
    unsigned char cmd[2] = {0x37,0x80,};

    hi_i2c_data sht3x_i2c_data = { 0 };
    sht3x_i2c_data.send_buf = cmd;
    sht3x_i2c_data.send_len = 2;
    sht3x_i2c_data.receive_buf = serialNumWords;
    sht3x_i2c_data.receive_len = sizeof(serialNumWords);
    
    ret = hi_i2c_write(0, ((unsigned char)0x44) << 1, &sht3x_i2c_data);
    if(ret != 0){
        DBG("hi_i2c_write failed ret :%#x \r\n",ret);
        return ret;
    }
    dump_buf(serialNumWords,sizeof(serialNumWords));
    
    ret = hi_i2c_read(0, ((unsigned char)0x44) << 1 | 0x01, &sht3x_i2c_data);
    if(ret != 0){
        DBG("hi_i2c_read failed ret :%#x \r\n",ret);
        return ret;
    }
    dump_buf(serialNumWords,sizeof(serialNumWords));

    ret = SHT3X_CheckCrc(serialNumWords,2,serialNumWords[2]);
    if(ret != NO_ERROR){
        DBG("read serial number crc check failed \r\n"); 
        return ret;
    }

    ret = SHT3X_CheckCrc(&serialNumWords[3],2,serialNumWords[5]);
    if(ret != NO_ERROR){
        DBG("read serial number crc check failed \r\n"); 
        return ret;
    }
    return ret;
}

static int SHT3x_WriteCMD(unsigned short cmd)
{
    int ret = -1;
    unsigned char sendbuf[2] = {0};
    unsigned char rcvbuf[2] = {0};
    
    hi_i2c_data sht3x_i2c_data = { 0 };
    sht3x_i2c_data.send_buf = sendbuf;
    sht3x_i2c_data.send_len = sizeof(sendbuf);
    sht3x_i2c_data.receive_buf = rcvbuf;
    sht3x_i2c_data.receive_len = sizeof(rcvbuf);
    
    sendbuf[0] = (cmd & 0xff00) >> 8;
    sendbuf[1] = cmd & 0xff;
    dump_buf(sendbuf,2); 
    ret = hi_i2c_write(0, ((unsigned char)0x44) << 1, &sht3x_i2c_data);
    if(ret != 0){
        DBG("hi_i2c_write failed ret :%#x \r\n",ret);
        return ret;
    }
    return 0;        
}

static int SHT3x_Read4BytesDataAndCrc(unsigned short *data)
{
    int ret = -1;
    unsigned char sendbuf[2] = {0};
    unsigned char rcvbuf[6] = {0};
    
    hi_i2c_data sht3x_i2c_data = { 0 };
    sht3x_i2c_data.send_buf = sendbuf;
    sht3x_i2c_data.send_len = sizeof(sendbuf);
    sht3x_i2c_data.receive_buf = rcvbuf;
    sht3x_i2c_data.receive_len = sizeof(rcvbuf);

    if(data == NULL){
        DBG("invalid para \r\n");
        return ret;
    }

    ret = hi_i2c_read(0, ((unsigned char)0x44) << 1 | 0x01, &sht3x_i2c_data);
    if(ret != 0){
        DBG("hi_i2c_read failed ret :%#x \r\n",ret);
        return ret;
    }
    
    ret = SHT3X_CheckCrc(rcvbuf,2,rcvbuf[2]);
    if(ret != NO_ERROR){
        DBG("read serial number crc check failed \r\n"); 
        return ret;
    }
    
    ret = SHT3X_CheckCrc(&rcvbuf[3],2,rcvbuf[5]);
    if(ret != NO_ERROR){
        DBG("read serial number crc check failed \r\n"); 
        return ret;
    }

    data[0] = rcvbuf[0] << 8 | rcvbuf[1];
    data[1] = rcvbuf[3] << 8 | rcvbuf[4];

    return 0;        
}

static int SHT3x_Read2BytesDataAndCrc(unsigned short *data)
{
    int ret = -1;
    unsigned char sendbuf[2] = {0};
    unsigned char rcvbuf[3] = {0};
    
    hi_i2c_data sht3x_i2c_data = { 0 };
    sht3x_i2c_data.send_buf = sendbuf;
    sht3x_i2c_data.send_len = sizeof(sendbuf);
    sht3x_i2c_data.receive_buf = rcvbuf;
    sht3x_i2c_data.receive_len = sizeof(rcvbuf);
    
    if(data == NULL){
        DBG("invalid para \r\n");
        return ret;
    }
   
    ret = hi_i2c_read(0, ((unsigned char)0x44) << 1 | 0x01, &sht3x_i2c_data);
    if(ret != 0){
        DBG("hi_i2c_read failed ret :%#x \r\n",ret);
        return ret;
    }
    
    ret = SHT3X_CheckCrc(rcvbuf,2,rcvbuf[2]);
    if(ret != NO_ERROR){
        DBG("read serial number crc check failed \r\n"); 
        return ret;
    }

    data[0] = rcvbuf[0] << 8 | rcvbuf[1];
    return 0;        
}

void SHT3X_SoftReset_t(void)
{
    SHT3x_WriteCMD(CMD_SOFT_RESET);
}

void SHT3X_ReadSerialNumber_t(void) 
{
    unsigned short data[2] = {0};
    SHT3X_SoftReset_t();
    SHT3x_WriteCMD(CMD_READ_SERIALNBR);

    SHT3x_Read4BytesDataAndCrc(data);
    printf("--------------------------------\r\n");
    dump_buf((unsigned char *)data,sizeof(data));
}

static void SHT3X_ReadStatus(unsigned short* status) 
{
    SHT3x_WriteCMD(CMD_READ_STATUS);
    SHT3x_Read2BytesDataAndCrc(status);
}

static void SHT3X_ClearAllAlertFlags(void)
{
    SHT3x_WriteCMD(CMD_CLEAR_STATUS);
}

/*
static void SHT3X_StartPeriodicMeasurment(void)
{
    // medium repeatability, 2.0 Hz
    SHT3x_WriteCMD(CMD_MEAS_PERI_2_M);
}
*/
static float SHT3X_CalcTemperature_t(unsigned short rawValue) 
{
    return 175.0f * (float)rawValue / 65535.0f - 45.0f; 
}

static float SHT3X_CalcHumidity_t(unsigned short rawValue) 
{
    return 100.0f * (float)rawValue / 65535.0f; 
}

void SHT3X_init(void)
{
    int ret = 0;
    unsigned short data[2] = {0};
    SHT3X_SoftReset_t();
    SHT3x_WriteCMD(CMD_READ_SERIALNBR);
    
    SHT3x_WriteCMD(CMD_MEAS_PERI_2_M);
    //SHT3X_StartPeriodicMeasurment();

}

void SHT3X_ReadMeasurementBuffer(float* temperature, float* humidity) 
{

    unsigned int rawValueTemp = 0; 
    
    SHT3x_WriteCMD(CMD_FETCH_DATA);
    SHT3x_Read4BytesDataAndCrc((unsigned short *)&rawValueTemp);

    dump_buf((unsigned char *)&rawValueTemp,sizeof(rawValueTemp));    

    *temperature = SHT3X_CalcTemperature_t(rawValueTemp);
    *humidity = SHT3X_CalcHumidity_t(*((unsigned short *)(&rawValueTemp)+1));
    Temperature_t = *temperature;
    Humidity_t = *humidity;
    DBG("temp :%f,hum :%f \r\n",Temperature_t,Humidity_t);
}

static void SHT3X_ReadMeasurementVal(unsigned int para)
{
    (void) para;
    static int cunt = 0;
    static float humidity = 0.0; 
    static float temperature = 0.0; 
    
    SHT3X_ReadMeasurementBuffer_t(&temperature,&humidity);
 
}
