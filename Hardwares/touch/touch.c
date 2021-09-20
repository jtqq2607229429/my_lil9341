//
// Created by JIANG on 2021/9/19.
//

#include "touch.h"
#include "delay.h"
#include "i2c.h"
#include "ILI93xx.h"
#include "main.h"
#include "stdio.h"

const uint8_t CMD_RDX = 0XD0;
const uint8_t CMD_RDY = 0X90;

float xfac, yfac;
int16_t xoff, yoff;
struct position pos;
uint8_t touchtype;
uint8_t sta;

void Set_CS_High() {
    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
}

void Set_CS_Low() {
    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_RESET);
}

void Set_MOSI_High() {
    HAL_GPIO_WritePin(TOUCH_MOSI_GPIO_Port, TOUCH_MOSI_Pin, GPIO_PIN_SET);
}

void Set_MOSI_Low() {
    HAL_GPIO_WritePin(TOUCH_MOSI_GPIO_Port, TOUCH_MOSI_Pin, GPIO_PIN_RESET);
}

void Set_SCK_High() {
    HAL_GPIO_WritePin(TOUCH_SCK_GPIO_Port, TOUCH_SCK_Pin, GPIO_PIN_SET);
}

void Set_SCK_Low() {
    HAL_GPIO_WritePin(TOUCH_SCK_GPIO_Port, TOUCH_SCK_Pin, GPIO_PIN_RESET);
}

uint8_t Read_MISO() {
    return HAL_GPIO_ReadPin(TOUCH_MISO_GPIO_Port, TOUCH_MISO_Pin) == GPIO_PIN_SET;
}

uint8_t Read_PEN() {
    return HAL_GPIO_ReadPin(TOUCH_PEN_GPIO_Port, TOUCH_PEN_Pin) == GPIO_PIN_SET;
}


void SPI_WriteByte(uint8_t Byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (Byte & 0x80) {
            Set_MOSI_High();
        } else {
            Set_MOSI_Low();
        }
        Byte <<= 1;
        Set_SCK_Low();
        HAL_delay_us(1);
        Set_SCK_High();
    }
}

//SPI读数据
//从触摸屏IC读取adc值
//CMD:指令
//返回值:读到的数据
uint16_t TP_Read_AD(uint8_t CMD) {
    uint8_t count = 0;
    uint16_t Num = 0;
    Set_SCK_Low();        //先拉低时钟
    Set_MOSI_Low();    //拉低数据线
    Set_CS_Low();        //选中触摸屏IC
    SPI_WriteByte(CMD);//发送命令字
    HAL_delay_us(6);//ADS7846的转换时间最长为6us
    Set_SCK_Low();
    HAL_delay_us(1);
    Set_SCK_High();        //给1个时钟，清除BUSY
    HAL_delay_us(1);
    Set_SCK_Low();
    for (count = 0; count < 16; count++)//读出16位数据,只有高12位有效
    {
        Num <<= 1;
        Set_SCK_Low();    //下降沿有效
        HAL_delay_us(1);
        Set_SCK_High();
        if (Read_MISO())Num++;
    }
    Num >>= 4;    //只有高12位有效.
    Set_CS_High();        //释放片选
    return (Num);
}

//读取一个坐标值(x或者y)
//连续读取READ_TIMES次数据,对这些数据升序排列,
//然后去掉最低和最高LOST_VAL个数,取平均值
//xy:指令（CMD_RDX/CMD_RDY）
//返回值:读到的数据
#define READ_TIMES 5    //读取次数
#define LOST_VAL 1        //丢弃值

uint16_t TP_Read_XOY(uint8_t xy) {
    uint16_t i, j;
    uint16_t buf[READ_TIMES];
    uint16_t sum = 0;
    uint16_t temp;
    for (i = 0; i < READ_TIMES; i++)buf[i] = TP_Read_AD(xy);
    for (i = 0; i < READ_TIMES - 1; i++)//排序
    {
        for (j = i + 1; j < READ_TIMES; j++) {
            if (buf[i] > buf[j])//升序排列
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }
    sum = 0;
    for (i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++)sum += buf[i];
    temp = sum / (READ_TIMES - 2 * LOST_VAL);
    return temp;
}

//读取x,y坐标
//最小值不能少于100.
//x,y:读取到的坐标值
//返回值:0,失败;1,成功。
uint8_t TP_Read_XY(uint16_t *x, uint16_t *y) {
    uint16_t xtemp, ytemp;
    xtemp = TP_Read_XOY(CMD_RDX);
    ytemp = TP_Read_XOY(CMD_RDY);
    //if(xtemp<100||ytemp<100)return 0;//读数失败
    *x = xtemp;
    *y = ytemp;
    return 1;//读数成功
}

//连续2次读取触摸屏IC,且这两次的偏差不能超过
//ERR_RANGE,满足条件,则认为读数正确,否则读数错误.
//该函数能大大提高准确度
//x,y:读取到的坐标值
//返回值:0,失败;1,成功。
#define ERR_RANGE 50 //误差范围

uint8_t TP_Read_XY2(uint16_t *x, uint16_t *y) {
    uint16_t x1, y1;
    uint16_t x2, y2;
    uint8_t flag;
    flag = TP_Read_XY(&x1, &y1);
    if (flag == 0)return (0);
    flag = TP_Read_XY(&x2, &y2);
    if (flag == 0)return (0);
    if (((x2 <= x1 && x1 < x2 + ERR_RANGE) || (x1 <= x2 && x2 < x1 + ERR_RANGE))//前后两次采样在+-50内
        && ((y2 <= y1 && y1 < y2 + ERR_RANGE) || (y1 <= y2 && y2 < y1 + ERR_RANGE))) {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;
    } else return 0;
}

//////////////////////////////////////////////////////////////////////////////////		  
//触摸按键扫描
//tp:0,屏幕坐标;1,物理坐标(校准等特殊场合用)
//返回值:当前触屏状态.
//0,触屏无触摸;1,触屏有触摸
uint8_t TP_Scan(uint8_t tp) {
    if (Read_PEN() == 0)//有按键按下
    {
        if (tp)TP_Read_XY2(&pos.x, &pos.y);//读取物理坐标
        else if (TP_Read_XY2(&pos.x, &pos.y))//读取屏幕坐标
        {
            pos.x = xfac * pos.x + xoff;//将结果转换为屏幕坐标
            pos.y = yfac * pos.y + yoff;
        }
    }
}

#define EEPROM_ADDR 0xA0
#define SAVE_ADDR_BASE 0x28

uint8_t TP_Get_Adjdata(void) {
    uint8_t temp8;
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, SAVE_ADDR_BASE + 13, I2C_MEMADD_SIZE_8BIT, &temp8, 1, 0xff);
    if (temp8 == 0X0A) {
        int32_t temp32;
        HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, SAVE_ADDR_BASE, I2C_MEMADD_SIZE_8BIT, (uint8_t *) &temp32, 4, 0xff);
        xfac = temp32 / 100000000.0;
        HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, SAVE_ADDR_BASE + 4, I2C_MEMADD_SIZE_8BIT, (uint8_t *) &temp32, 4, 0xff);
        yfac = temp32 / 100000000.0;
        int16_t temp16;
        HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, SAVE_ADDR_BASE + 8, I2C_MEMADD_SIZE_8BIT, (uint8_t *) &temp16, 2, 0xff);
        xoff = temp16;
        HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, SAVE_ADDR_BASE + 10, I2C_MEMADD_SIZE_8BIT, (uint8_t *) &temp16, 2, 0xff);
        yoff = temp16;
        HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, SAVE_ADDR_BASE + 12, I2C_MEMADD_SIZE_8BIT, &temp8, 1, 0xff);
        touchtype = temp8;
        return 1;
    }
    return 0;
}

void TP_Drow_Touch_Point(uint16_t x, uint16_t y, uint16_t color) {
    POINT_COLOR = color;
    LCD_DrawLine(x - 12, y, x + 13, y);
    LCD_DrawLine(x, y - 12, x, y + 13);
    LCD_DrawPoint(x + 1, y + 1);
    LCD_DrawPoint(x - 1, y + 1);
    LCD_DrawPoint(x + 1, y - 1);
    LCD_DrawPoint(x - 1, y - 1);
    LCD_Draw_Circle(x, y, 6);
}

uint8_t TP_Init(void) {
    TP_Read_XY(&pos.x, &pos.y);
    if (TP_Get_Adjdata()) {
        return 0;
    }
    TP_Get_Adjdata();
    return 1;
}