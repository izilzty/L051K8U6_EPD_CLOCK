#ifndef _LUNAR_H_
#define _LUNAR_H_

#include "main.h"

struct Lunar_Date
{
    uint8_t IsLeap;
    uint8_t Date;
    uint8_t Month;
    uint16_t Year;
};

const static char Lunar_MonthString[13][7] = {
    "未知",
    "正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月",
    "冬月", "腊月"};

const static char Lunar_MonthLeapString[2][4] = {
    "",
    "润"};

const static char Lunar_DateString[31][7] = {
    "未知",
    "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"};

const static char Lunar_DayString[8][4] = {
    "  ",
    "一", "二", "三", "四", "五", "六", "日"};

const static char Lunar_ZodiacString[12][4] = {
    "猴", "鸡", "狗", "猪", "鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊"};

const static char Lunar_StemStrig[10][4] = {
    "庚", "辛", "壬", "癸", "甲", "乙", "丙", "丁", "戊", "已"};

const static char Lunar_BranchStrig[12][4] = {
    "申", "酉", "戌", "亥", "子", "丑", "寅", "卯", "辰", "巳", "午", "未"};

void LUNAR_SolarToLunar(struct Lunar_Date *lunar, uint16_t solar_year, uint8_t solar_month, uint8_t solar_date);
uint8_t LUNAR_GetZodiac(const struct Lunar_Date *lunar);
uint8_t LUNAR_GetStem(const struct Lunar_Date *lunar);
uint8_t LUNAR_GetBranch(const struct Lunar_Date *lunar);

#endif
